/**
 * 实时数据通道（轮询版）
 *   单片机 -> ThingsCloud MQTT -> Python 后端(内存缓存) -> 轮询 /devices
 *
 * 新后端（backend/app.py）没有 WebSocket，固件 6s 上报一次，
 * 页面端每 5 秒轮询一次足够实时。
 * 对外保持 subscribe / start / stop / refresh / getState 接口不变，
 * 页面代码无需关心数据是推送还是轮询来的。
 */
const api = require('./api.js')

const POLL_INTERVAL = 5000

const state = {
  connected: false, // 最近一次轮询是否成功
  devices: [], // 设备列表 [{id, name, online, alarm, data}]
  lastUpdate: 0, // 最近一次拿到数据的时间
  source: 'poll'
}

const listeners = []
let pollTimer = null
let polling = false
let started = false

function emit() {
  for (let i = 0; i < listeners.length; i++) {
    try {
      listeners[i](state)
    } catch (e) {
      console.error('[store] 订阅回调异常', e)
    }
  }
}

function poll() {
  if (polling) return Promise.resolve(state)
  polling = true
  return api
    .devices()
    .then(list => {
      state.devices = list || []
      state.lastUpdate = Date.now()
      state.connected = true
      emit()
    })
    .catch(() => {
      // 失败不回调刷屏，只有状态从连上变为断开时通知一次
      if (state.connected) {
        state.connected = false
        emit()
      }
    })
    .then(() => {
      polling = false
    })
}

/* --------------------------- 对外接口 --------------------------- */

function start() {
  started = true
  poll()
  if (!pollTimer) pollTimer = setInterval(poll, POLL_INTERVAL)
}

function stop() {
  started = false
  if (pollTimer) {
    clearInterval(pollTimer)
    pollTimer = null
  }
  state.connected = false
}

/** 立即拉一次数据（下拉刷新用） */
function refresh() {
  poll()
  return Promise.resolve(state)
}

/** 订阅全局状态，返回取消订阅函数 */
function subscribe(fn) {
  listeners.push(fn)
  fn(state)
  if (!started) start()
  return () => {
    const i = listeners.indexOf(fn)
    if (i >= 0) listeners.splice(i, 1)
  }
}

function getState() {
  return state
}

function getDevice(id) {
  return state.devices.find(d => d.id === id) || null
}

module.exports = {
  state,
  start,
  stop,
  refresh,
  subscribe,
  getState,
  getDevice
}
