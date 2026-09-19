/**
 * 后端 REST 接口封装
 * 后端：backend/app.py（FastAPI，Classroom Monitor API）
 *   GET    /devices                       设备列表（含最新数据、在线与报警状态）
 *   GET    /devices/{id}/history?limit=   历史数据 [{ts, Temp, Humi, ...}]
 *   GET    /devices/{id}/thresholds       当前报警阈值
 *   PUT    /devices/{id}/thresholds        下发阈值 {A_Temp, A_Hum, ...}
 *   POST   /devices/{id}/control          RGB 灯控制 {R, G, B}
 *   POST   /devices/{id}/analyze           AI 环境分析 {question, minutes}
 */

const config = require('./config.js')

function baseUrl() {
  const app = getApp()
  return (app && app.globalData && app.globalData.baseUrl) || config.BASE_URL
}

function request(path, method, data, timeout) {
  return new Promise((resolve, reject) => {
    wx.request({
      url: baseUrl() + path,
      method: method || 'GET',
      data: data || {},
      header: {
        'Content-Type': 'application/json'
      },
      timeout: timeout || 10000,
      success(res) {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(res.data)
        } else {
          const detail = res.data && res.data.detail ? `：${res.data.detail}` : ''
          reject(new Error(`HTTP ${res.statusCode}${detail}`))
        }
      },
      fail(err) {
        reject(new Error(err.errMsg || '网络请求失败'))
      }
    })
  })
}

const enc = encodeURIComponent

module.exports = {
  /**
   * 设备列表：[{id, name, online, alarm, data:{Temp, Humi, ..., A_Temp, ts}}]
   * data 为合并后的最新属性（含阈值），ts 为最近一次上报时间戳(ms)
   */
  devices: () => request('/devices'),

  /** 历史数据：[{ts, Temp, Humi, ...}]，固件 6s 一条 */
  history: (id, limit = 1000) =>
    request(`/devices/${enc(id)}/history?limit=${limit}`),

  /** 当前报警阈值：{A_Temp, A_Hum, A_Pre, A_GZ_Value, A_MQ2_Value, A_MQ7_Value, A_MQ135_Value} */
  thresholds: id => request(`/devices/${enc(id)}/thresholds`),

  /**
   * 下发报警阈值（只传要改的字段，整型）
   * @param {object} body 例如 {A_Temp: 30, A_Hum: 25}
   */
  setThresholds: (id, body) =>
    request(`/devices/${enc(id)}/thresholds`, 'PUT', body),

  /**
   * RGB 灯控制
   * @param {object} payload 例如 {R: true} 或 {G: false}
   */
  control: (id, payload) =>
    request(`/devices/${enc(id)}/control`, 'POST', payload),

  /**
   * AI 环境分析（推理模型通常需要 10~30 秒，超时放宽到 180s）
   * @param {object} body {question: '可选追问', minutes: 分析最近多少分钟}
   */
  analyze: (id, body) =>
    request(`/devices/${enc(id)}/analyze`, 'POST', body, 180000),

  /** 便于页面提示 */
  baseUrl
}
