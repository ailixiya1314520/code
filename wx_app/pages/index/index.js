/**
 * 首页：信号场总览
 * 数据来自 GET /devices：[{id, name, online, alarm, data}]
 * 页面结构与网页版 frontend/index.html 的 nav + strip + 01 教室列表对应
 */
const socket = require('../../utils/socket.js')
const fmt = require('../../utils/format.js')

Page({
  data: {
    connected: false,
    onlineCount: 0,
    total: 0,
    alarmCount: 0,
    clock: '',
    avgTemp: '--',
    avgHumi: '--',
    lastText: '--',
    rooms: []
  },

  onLoad() {
    this.unsubscribe = socket.subscribe(s => this.apply(s))
  },

  onShow() {
    socket.start()
    socket.refresh()
    this.tick()
    this.timer = setInterval(() => this.tick(), 1000)
  },

  onHide() {
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
  },

  onUnload() {
    this.onHide()
    if (this.unsubscribe) this.unsubscribe()
  },

  onPullDownRefresh() {
    socket.refresh()
    setTimeout(() => wx.stopPullDownRefresh(), 600)
  },

  tick() {
    const d = new Date()
    const p = n => (n < 10 ? '0' + n : '' + n)
    this.setData({
      clock: `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`
    })
  },

  /** 把全局状态映射为页面数据 */
  apply(s) {
    const rooms = (s.devices || []).map(d => {
      const data = d.data || {}
      return {
        id: d.id,
        name: d.name || d.id,
        online: !!d.online,
        alarm: !!d.alarm,
        temp: fmt.fmtNum(data.Temp),
        humi: fmt.fmtNum(data.Humi),
        ago: fmt.ago(data.ts)
      }
    })

    let ts = 0
    let tn = 0
    let hs = 0
    let hn = 0
    let lastTs = 0
    rooms.forEach(r => {
      const t = Number(r.temp)
      const h = Number(r.humi)
      if (!isNaN(t)) { ts += t; tn++ }
      if (!isNaN(h)) { hs += h; hn++ }
    })
    ;(s.devices || []).forEach(d => {
      const dts = d.data && d.data.ts
      if (dts && dts > lastTs) lastTs = dts
    })

    this.setData({
      connected: !!s.connected,
      total: rooms.length,
      onlineCount: rooms.filter(r => r.online).length,
      alarmCount: rooms.filter(r => r.alarm).length,
      lastText: lastTs ? fmt.fmtDateTime(lastTs) : '--',
      avgTemp: tn ? (ts / tn).toFixed(1) : '--',
      avgHumi: hn ? (hs / hn).toFixed(1) : '--',
      rooms: rooms
    })
  },

  openDetail(e) {
    const id = e.currentTarget.dataset.id
    wx.navigateTo({ url: `/pages/detail/detail?id=${encodeURIComponent(id)}` })
  },

  goAlarms() {
    wx.navigateTo({ url: '/pages/alarms/alarms' })
  }
})
