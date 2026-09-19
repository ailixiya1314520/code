/**
 * 趋势曲线组件（canvas 2d 绘制，等价于 Web 大屏的 TrendChart）
 * 用法：<trend list="{{series}}" field="Temp" color="#ffb63b" unit="°C" />
 * list 为后端 /history 返回的原始数组 [{ts, Temp, Humi, ...}]
 */
const chart = require('../../utils/chart.js')

Component({
  properties: {
    list: { type: Array, value: [], observer() { this.draw() } },
    field: { type: String, value: 'Temp', observer() { this.draw() } },
    color: { type: String, value: '#00e6ff', observer() { this.draw() } },
    unit: { type: String, value: '', observer() { this.draw() } }
  },

  data: {},

  lifetimes: {
    ready() { this.init() },
    detached() { this.ctx = null }
  },

  methods: {
    init() {
      wx.createSelectorQuery()
        .in(this)
        .select('#trend-canvas')
        .fields({ node: true, size: true })
        .exec(res => {
          const info = res && res[0]
          if (!info || !info.node) return
          const dpr = chart.getDPR()
          const canvas = info.node
          const ctx = canvas.getContext('2d')
          canvas.width = info.width * dpr
          canvas.height = info.height * dpr
          ctx.scale(dpr, dpr)
          this.canvas = canvas
          this.ctx = ctx
          this.size = { w: info.width, h: info.height }
          this.draw()
        })
    },

    draw() {
      if (!this.ctx || !this.size) return
      const field = this.data.field
      const points = (this.data.list || [])
        .map(p => ({ ts: p.ts, v: Number(p[field]) }))
        .filter(p => !isNaN(p.v))

      chart.drawLine(this.ctx, this.size.w, this.size.h, points, {
        color: this.data.color
      })
    }
  }
})
