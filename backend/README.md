# 教室环境监测后端

```
pip install -r requirements.txt
cp .env.example .env      # 填入 ThingsCloud 与 AI 密钥
uvicorn app:app --host 0.0.0.0 --port 8000
```

大屏: http://localhost:8000  交互文档: http://localhost:8000/docs

## Docker 部署 (1Panel)

push 到 main 后 GitHub Actions 自动构建镜像 `ghcr.io/ailixiya1314520/code:latest`。
1Panel 里 容器 > 编排 > 创建, 粘贴仓库根目录的 `docker-compose.yml`, 环境变量按 `.env.example` 填写。
镜像若为私有, 先在 1Panel 仓库里添加 ghcr.io 并用 GitHub Token 登录。

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | /devices | 全部设备: 在线状态、是否报警、最新读数与阈值 |
| GET | /devices/{id}/history?limit=200 | 最近 N 条上报 (内存, 重启清空) |
| GET | /devices/{id}/thresholds | 当前报警阈值 |
| PUT | /devices/{id}/thresholds | 下发阈值, 例 `{"A_Temp":35,"A_Hum":20}`, 设备实时生效并保存在云端 |
| POST | /devices/{id}/control | RGB 灯控制, 例 `{"R":true,"B":false}` |
| POST | /devices/{id}/analyze | AI 分析, 例 `{"minutes":60,"question":"现在适合上课吗"}` |

`id` 为 ThingsCloud 设备 ID (固件 `TC_CLIENTID`), 见 `.env` 的 `DEVICES`。
ThingsCloud 对同一设备的下发有限频, 4 秒内多次会返回 403。
