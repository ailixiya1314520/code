/**
 * 告警页 —— 实时越限状态
 * 新后端没有告警历史接口，此页根据 GET /devices 的 alarm 标志
 * 实时展示哪些教室正在报警、哪些指标越限。
 */
const socket = require('../../utils/socket.js')
const fmt = require('../../utils/format.js')

Page({
  data: {
    total: 0,
    alarmCount: 0,
    offlineCount: 0,
    rooms: []
  },

  onLoad() {
    this.unsubscribe = socket.subscribe(s => this.apply(s))
  },

  onShow() {
    socket.start()
    socket.refresh()
  },

  onPullDownRefresh() {
    socket.refresh()
    setTimeout(() => wx.stopPullDownRefresh(), 600)
  },

  onUnload() {
    if (this.unsubscribe) this.unsubscribe()
  },

  apply(s) {
    const rooms = (s.devices || []).map(d => {
      const data = d.data || {}
      const overs = fmt.buildMetrics(data).filter(m => m.over).map(m => ({
        key: m.key,
        label: m.label,
        text: `当前 ${m.value} ${m.unit} · ${m.thresholdLabel} ${m.threshold}`
      }))
      return {
        id: d.id,
        name: d.name || d.id,
        online: !!d.online,
        alarm: !!d.alarm,
        overs: overs,
        timeText: data.ts ? fmt.fmtDateTime(data.ts) : '--',
        ago: fmt.ago(data.ts)
      }
    })

    this.setData({
      total: rooms.length,
      alarmCount: rooms.filter(r => r.alarm).length,
      offlineCount: rooms.filter(r => !r.online).length,
      rooms: rooms
    })
  },

  openDetail(e) {
    const id = e.currentTarget.dataset.id
    if (!id) return
    wx.navigateTo({ url: `/pages/detail/detail?id=${encodeURIComponent(id)}` })
  }
})
