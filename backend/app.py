"""教室环境监测 Python 后端

  数据来源  ThingsCloud MQTT 应用端订阅(wss), 内存缓存最新值 + 近期历史
  设备控制  ThingsCloud 应用端设备访问 API (HTTP), 下发阈值 / RGB 灯
  AI 分析   OpenAI 兼容接口 (deepseek-v4-flash)

运行:  uvicorn app:app --host 0.0.0.0 --port 8000
文档:  http://localhost:8000/docs
配置:  同目录 .env (见 .env.example)
"""
import json
import os
import ssl
import statistics
import threading
import time
from collections import deque
from pathlib import Path
from urllib.parse import urlparse

import paho.mqtt.client as mqtt
import requests
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

# ---------------- config ----------------
_ENV = Path(__file__).parent / ".env"          # 本地开发用; Docker 里直接由环境变量注入
for _line in _ENV.read_text(encoding="utf-8").splitlines() if _ENV.exists() else ():
    if "=" in _line and not _line.startswith("#"):
        _k, _v = _line.split("=", 1)
        os.environ.setdefault(_k.strip(), _v.strip())

TC_API         = os.environ["TC_API"]
TC_PROJECT_KEY = os.environ["TC_PROJECT_KEY"]
TC_VIEW_URL    = os.environ["TC_VIEW_URL"]
TC_VIEW_KEY    = os.environ["TC_VIEW_KEY"]
TC_VIEW_SECRET = os.environ["TC_VIEW_SECRET"]
AI_BASE        = os.environ["AI_BASE"]
AI_KEY         = os.environ["AI_KEY"]
AI_MODEL       = os.environ["AI_MODEL"]
DEVICES        = json.loads(os.environ["DEVICES"])   # {"<ThingsCloud 设备ID>": {"name": ..., "token": "<AccessToken>"}}

THRESHOLD_KEYS = ("A_Temp", "A_Hum", "A_Pre", "A_GZ_Value", "A_MQ2_Value", "A_MQ7_Value", "A_MQ135_Value")
SENSOR_KEYS    = ("Temp", "Humi", "Pre", "GZ_Value", "MQ2_Value", "MQ7_Value", "MQ135_Value")

# ---------------- state ----------------
# ponytail: 纯内存, 重启即清; 需要断电保留历史时换 sqlite
latest  = {d: {} for d in DEVICES}                 # 合并后的最新属性(含阈值)
history = {d: deque(maxlen=5000) for d in DEVICES}  # (ts_ms, attributes), 6s 一条约 8 小时
lock    = threading.Lock()
ONLINE_MS = 30_000   # 固件 6s 上报一次; 超过 30s 没上报视为离线(平台对异常掉线不发离线事件)

# ---------------- ThingsCloud HTTP ----------------
_H = {"Project-Key": TC_PROJECT_KEY}


def _dev(dev: str) -> dict:
    if dev not in DEVICES:
        raise HTTPException(404, f"unknown device {dev}")
    return DEVICES[dev]


def tc_get(dev: str) -> dict:
    r = requests.get(f"{TC_API}/app/device/v1/{_dev(dev)['token']}/attributes", headers=_H, timeout=10)
    r.raise_for_status()
    return r.json().get("attributes", {})


def tc_push(dev: str, attrs: dict) -> dict:
    """下发属性到设备, 云端同时保存; 设备端在 attributes/push 收到同样的 JSON"""
    r = requests.post(f"{TC_API}/app/device/v1/{_dev(dev)['token']}/attributes", headers=_H, json=attrs, timeout=10)
    r.raise_for_status()
    with lock:
        latest[dev].update(attrs)
    return r.json()


def alarm(a: dict) -> bool:
    """与固件 main.c 相同的报警规则"""
    g = a.get
    return (g("Temp", 0) >= g("A_Temp", 35) or g("Humi", 100) <= g("A_Hum", 20) or
            g("Pre", 0) >= g("A_Pre", 1500) or g("MQ135_Value", 9999) <= g("A_MQ135_Value", 500) or
            g("MQ2_Value", 0) >= g("A_MQ2_Value", 3500) or g("MQ7_Value", 0) >= g("A_MQ7_Value", 3500))


# ---------------- ThingsCloud MQTT 应用端订阅 ----------------
def _on_message(_c, _u, m):
    b = json.loads(m.payload)
    dev = b["device"]["id"]
    if dev not in DEVICES:
        return
    with lock:
        latest[dev].update(b["attributes"])
        latest[dev]["ts"] = b["ts"]
        history[dev].append((b["ts"], b["attributes"]))


def _mqtt_loop():
    u = urlparse(TC_VIEW_URL)
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, transport="websockets", protocol=mqtt.MQTTv311)
    c.ws_set_options(path=u.path)
    c.tls_set_context(ssl.create_default_context())
    c.username_pw_set(TC_VIEW_KEY, TC_VIEW_SECRET)
    c.on_connect = lambda c, *_: c.subscribe(f"{TC_VIEW_KEY}/+/attributes", 0)
    c.on_message = _on_message
    c.connect(u.hostname, u.port or 443, 60)
    c.loop_forever(retry_first_connection=True)


for _d in DEVICES:                       # 启动先用 HTTP 拉一次, 阈值和最新值立即可用
    try:
        latest[_d].update(tc_get(_d))
    except Exception as e:               # 云端不通也照常启动, 等 MQTT 数据
        print(f"seed {_d} failed: {e}")
threading.Thread(target=_mqtt_loop, daemon=True).start()

# ---------------- HTTP API ----------------
app = FastAPI(title="Classroom Monitor API")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])


class Thresholds(BaseModel):
    A_Temp: int | None = None
    A_Hum: int | None = None
    A_Pre: int | None = None
    A_GZ_Value: int | None = None
    A_MQ2_Value: int | None = None
    A_MQ7_Value: int | None = None
    A_MQ135_Value: int | None = None


class Control(BaseModel):
    R: bool | None = None      # RGB 灯三路开关
    G: bool | None = None
    B: bool | None = None


class Analyze(BaseModel):
    question: str = ""     # 可选追问, 例如 "现在适合上课吗"
    minutes: int = 60      # 分析最近多少分钟的数据


@app.get("/devices")
def list_devices():
    now = time.time() * 1000
    with lock:
        return [{"id": d, "name": DEVICES[d]["name"],
                 "online": now - latest[d].get("ts", 0) < ONLINE_MS,
                 "alarm": alarm(latest[d]), "data": dict(latest[d])} for d in DEVICES]


@app.get("/devices/{dev}/history")
def get_history(dev: str, limit: int = 200):
    _dev(dev)
    with lock:
        rows = list(history[dev])[-limit:]
    return [{"ts": ts, **a} for ts, a in rows]


@app.get("/devices/{dev}/thresholds")
def get_thresholds(dev: str):
    _dev(dev)
    with lock:
        return {k: latest[dev][k] for k in THRESHOLD_KEYS if k in latest[dev]}


@app.put("/devices/{dev}/thresholds")
def set_thresholds(dev: str, body: Thresholds):
    attrs = body.model_dump(exclude_none=True)
    if not attrs:
        raise HTTPException(400, "no threshold given")
    try:
        return tc_push(dev, attrs)
    except requests.RequestException as e:
        raise HTTPException(502, f"ThingsCloud: {e}")


@app.post("/devices/{dev}/control")
def control(dev: str, body: Control):
    attrs = body.model_dump(exclude_none=True)
    if not attrs:
        raise HTTPException(400, "nothing to control")
    try:
        return tc_push(dev, attrs)
    except requests.RequestException as e:
        raise HTTPException(502, f"ThingsCloud: {e}")


@app.post("/devices/{dev}/analyze")
def analyze(dev: str, body: Analyze):
    name = _dev(dev)["name"]
    since = (time.time() - body.minutes * 60) * 1000
    with lock:
        cur = dict(latest[dev])
        rows = [r for r in history[dev] if r[0] >= since]
    stats = {}
    for k in SENSOR_KEYS:
        vals = [r[1][k] for r in rows if k in r[1]]
        if vals:
            stats[k] = {"min": min(vals), "max": max(vals), "avg": round(statistics.fmean(vals), 1), "n": len(vals)}
    thresholds = {k: cur[k] for k in THRESHOLD_KEYS if k in cur}
    readings = {k: cur[k] for k in SENSOR_KEYS if k in cur}

    prompt = (
        f"你是教室环境监测系统的分析助手, 设备: {name}。\n"
        f"当前读数: {json.dumps(readings, ensure_ascii=False)}\n"
        f"报警阈值: {json.dumps(thresholds, ensure_ascii=False)}\n"
        f"最近 {body.minutes} 分钟统计: {json.dumps(stats, ensure_ascii=False)}\n"
        "字段说明: Temp 温度°C, Humi 湿度%, Pre 气压hPa, GZ_Value 光照, MQ2 烟雾/可燃气, "
        "MQ7 一氧化碳, MQ135 空气质量, 后四项为 ADC 原始值 0-4095, MQ135 数值越低空气越差。\n"
        "报警规则: 温度>=A_Temp 或 湿度<=A_Hum 或 气压>=A_Pre 或 MQ2>=A_MQ2_Value 或 "
        "MQ7>=A_MQ7_Value 或 MQ135<=A_MQ135_Value 时蜂鸣器报警; 当前"
        + ("已报警" if alarm(cur) else "未报警") + "。\n"
        "请用中文简明输出: 1) 当前环境评估 2) 异常或趋势 3) 建议(通风/窗帘/阈值调整等)。"
        + (f"\n用户追问: {body.question}" if body.question else "")
    )
    try:
        r = requests.post(f"{AI_BASE}/chat/completions", headers={"Authorization": f"Bearer {AI_KEY}"},
                          json={"model": AI_MODEL, "messages": [{"role": "user", "content": prompt}],
                                "max_tokens": 4000, "temperature": 0.3}, timeout=180)
        r.raise_for_status()
        choice = r.json()["choices"][0]
    except (requests.RequestException, KeyError, ValueError) as e:
        raise HTTPException(502, f"AI: {e}")
    if not choice["message"].get("content"):
        raise HTTPException(502, f"AI 无正文输出, finish_reason={choice.get('finish_reason')}")
    return {"device": dev, "analysis": choice["message"]["content"], "readings": readings,
            "thresholds": thresholds, "stats": stats, "alarm": alarm(cur)}


# 大屏与 API 同源, 放最后以免盖住上面的路由
app.mount("/", StaticFiles(directory=Path(__file__).parent.parent / "frontend", html=True))
