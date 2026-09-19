/**
 * 仪表盘组件（canvas 2d 绘制，等价于 Web 大屏的 GaugeCard）
 * 用法：<gauge title="温度" value="{{v}}" min="0" max="50" unit="°C" color="#ffb63b" ideal="{{[22,26]}}" />
 */
const chart = require('../../utils/chart.js')

Component({
  properties: {
    title: { type: String, value: '' },
    value: { type: null, value: null, observer() { this.draw() } },
    min: { type: Number, value: 0, observer() { this.draw() } },
    max: { type: Number, value: 100, observer() { this.draw() } },
    unit: { type: String, value: '', observer() { this.draw() } },
    color: { type: String, value: '#00e6ff', observer() { this.draw() } },
    ideal: { type: Array, value: [], observer() { this.draw() } }
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
        .select('#gauge-canvas')
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
      chart.drawGauge(this.ctx, this.size.w, this.size.h, {
        title: this.data.title,
        value: this.data.value,
        min: this.data.min,
        max: this.data.max,
        unit: this.data.unit,
        color: this.data.color,
        ideal: this.data.ideal
      })
    }
  }
})
