/**
 * 轻量折线图：直接在 canvas 2d 上下文上绘制时序曲线
 * 避免引入 echarts-for-weixin，减小小程序体积
 */

function fmtClock(ts) {
  if (!ts) return '--:--'
  const d = new Date(ts)
  const p = n => (n < 10 ? '0' + n : '' + n)
  return `${p(d.getHours())}:${p(d.getMinutes())}`
}

function fmtNum(v) {
  const n = Math.abs(v)
  if (n >= 1000) return String(Math.round(v))
  if (n >= 100) return String(Math.round(v))
  return String(Math.round(v * 10) / 10)
}

/**
 * @param {CanvasRenderingContext2D} ctx
 * @param {number} w 逻辑宽
 * @param {number} h 逻辑高
 * @param {Array<{ts:number, v:number}>} points
 * @param {object} opts {color, fill}
 */
function render(ctx, w, h, points, opts) {
  opts = opts || {}
  const color = opts.color || '#38BDF8'   // 网页版数据标记色，单一色相
  const padL = 38
  const padR = 12
  const padT = 14
  const padB = 24
  const cw = w - padL - padR
  const ch = h - padT - padB

  ctx.clearRect(0, 0, w, h)
  // 背景：近黑 + 极淡表面色（网页版 .ai-out 同款）
  ctx.fillStyle = 'rgba(255, 255, 255, 0.015)'
  ctx.fillRect(0, 0, w, h)

  const valid = (points || []).filter(p => typeof p.v === 'number' && !isNaN(p.v))

  if (!valid.length) {
    ctx.fillStyle = '#646C7E'
    ctx.font = '12px monospace'
    ctx.textAlign = 'center'
    ctx.textBaseline = 'middle'
    ctx.fillText('暂无数据', w / 2, h / 2)
    return
  }

  let min = Infinity
  let max = -Infinity
  valid.forEach(p => {
    if (p.v < min) min = p.v
    if (p.v > max) max = p.v
  })
  if (max - min < 1e-6) {
    max = min + 1
    min = min - 1
  }
  const margin = (max - min) * 0.12
  min -= margin
  max += margin

  const span = valid.length === 1 ? 1 : valid.length - 1
  const X = i => padL + (cw * i) / span
  const Y = v => padT + ch - ((v - min) / (max - min)) * ch

  // 横向网格 + Y 轴刻度
  ctx.lineWidth = 1
  ctx.font = '10px monospace'
  ctx.textAlign = 'right'
  ctx.textBaseline = 'middle'
  for (let i = 0; i <= 3; i++) {
    const yy = padT + (ch * i) / 3
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.09)'
    ctx.beginPath()
    ctx.moveTo(padL, yy)
    ctx.lineTo(padL + cw, yy)
    ctx.stroke()
    ctx.fillStyle = '#646C7E'
    ctx.fillText(fmtNum(max - ((max - min) * i) / 3), padL - 6, yy)
  }

  // 面积填充
  if (opts.fill !== false) {
    ctx.beginPath()
    ctx.moveTo(X(0), Y(valid[0].v))
    valid.forEach((p, i) => ctx.lineTo(X(i), Y(p.v)))
    ctx.lineTo(X(valid.length - 1), padT + ch)
    ctx.lineTo(X(0), padT + ch)
    ctx.closePath()
    const g = ctx.createLinearGradient(0, padT, 0, padT + ch)
    g.addColorStop(0, hexToRgba(color, 0.35))
    g.addColorStop(1, hexToRgba(color, 0))
    ctx.fillStyle = g
    ctx.fill()
  }

  // 折线
  ctx.beginPath()
  valid.forEach((p, i) => {
    if (i === 0) ctx.moveTo(X(i), Y(p.v))
    else ctx.lineTo(X(i), Y(p.v))
  })
  ctx.strokeStyle = color
  ctx.lineWidth = 2
  ctx.lineJoin = 'round'
  ctx.stroke()

  // 末端圆点
  const last = valid[valid.length - 1]
  ctx.beginPath()
  ctx.arc(X(valid.length - 1), Y(last.v), 3, 0, Math.PI * 2)
  ctx.fillStyle = color
  ctx.fill()

  // 起止时间
  ctx.fillStyle = '#646C7E'
  ctx.font = '10px monospace'
  ctx.textBaseline = 'bottom'
  ctx.textAlign = 'left'
  ctx.fillText(fmtClock(valid[0].ts), padL, h - 6)
  ctx.textAlign = 'right'
  ctx.fillText(fmtClock(last.ts), padL + cw, h - 6)
}

/**
 * 仪表盘（等价于 Web 大屏 GaugeCard 里的 echarts gauge）
 * @param {CanvasRenderingContext2D} ctx
 * @param {number} w 逻辑宽
 * @param {number} h 逻辑高
 * @param {object} opt {title, value, min, max, unit, color, ideal:[lo,hi]}
 */
function drawGauge(ctx, w, h, opt) {
  opt = opt || {}
  const min = typeof opt.min === 'number' ? opt.min : 0
  const max = typeof opt.max === 'number' ? opt.max : 100
  const color = opt.color || '#38BDF8'
  const unit = opt.unit || ''
  const ideal = opt.ideal || []

  let value = null
  if (opt.value !== null && opt.value !== undefined && opt.value !== '') {
    const n = Number(opt.value)
    if (!isNaN(n)) value = n
  }

  ctx.clearRect(0, 0, w, h)

  const cx = w / 2
  const cy = h * 0.6
  const r = Math.max(16, Math.min(w * 0.4, h * 0.44))
  const lw = Math.max(5, r * 0.16)

  // 起始 150°，顺时针扫过 240°，缺口留在正下方
  const START = (150 * Math.PI) / 180
  const SWEEP = (240 * Math.PI) / 180
  const frac = v => Math.min(1, Math.max(0, (v - min) / (max - min || 1)))

  if (opt.title) {
    ctx.fillStyle = '#98A1B3'
    ctx.font = '11px monospace'
    ctx.textAlign = 'left'
    ctx.textBaseline = 'top'
    ctx.fillText(opt.title, 6, 4)
  }

  ctx.lineCap = 'round'

  // 底环
  ctx.beginPath()
  ctx.arc(cx, cy, r, START, START + SWEEP)
  ctx.strokeStyle = 'rgba(255,255,255,0.10)'
  ctx.lineWidth = lw
  ctx.stroke()

  // 舒适区间
  if (ideal.length === 2) {
    const a0 = START + frac(ideal[0]) * SWEEP
    const a1 = START + frac(ideal[1]) * SWEEP
    if (a1 > a0) {
      ctx.beginPath()
      ctx.arc(cx, cy, r, a0, a1)
      ctx.strokeStyle = hexToRgba('#34D399', 0.4)
      ctx.stroke()
    }
  }

  // 当前值
  if (value !== null) {
    const a = START + frac(value) * SWEEP
    ctx.beginPath()
    ctx.arc(cx, cy, r, START, Math.max(a, START + 0.004))
    ctx.strokeStyle = color
    ctx.stroke()

    const px = cx + Math.cos(a) * r
    const py = cy + Math.sin(a) * r
    ctx.beginPath()
    ctx.arc(px, py, Math.max(2, lw * 0.3), 0, Math.PI * 2)
    ctx.fillStyle = color
    ctx.fill()
  }

  // 量程端点
  const rr = r + lw * 0.5 + 8
  ctx.fillStyle = '#646C7E'
  ctx.font = '10px monospace'
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.fillText(fmtNum(min), cx + Math.cos(START) * rr, cy + Math.sin(START) * rr)
  ctx.fillText(fmtNum(max), cx + Math.cos(START + SWEEP) * rr, cy + Math.sin(START + SWEEP) * rr)

  // 中心数值
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.fillStyle = value === null ? '#646C7E' : '#F4F6FB'
  ctx.font = `300 ${Math.round(Math.max(14, r * 0.52))}px monospace`
  ctx.fillText(value === null ? '--' : fmtNum(value), cx, cy - r * 0.08)

  if (unit) {
    ctx.fillStyle = '#646C7E'
    ctx.font = '10px monospace'
    ctx.fillText(unit, cx, cy + r * 0.42)
  }
}

function hexToRgba(hex, alpha) {
  let h = String(hex).replace('#', '')
  if (h.length === 3) {
    h = h[0] + h[0] + h[1] + h[1] + h[2] + h[2]
  }
  const num = parseInt(h, 16)
  const r = (num >> 16) & 255
  const g = (num >> 8) & 255
  const b = num & 255
  return `rgba(${r},${g},${b},${alpha})`
}

/** 设备像素比，canvas 需要按 dpr 放大后再 scale */
function getDPR() {
  try {
    const info = wx.getWindowInfo ? wx.getWindowInfo() : wx.getSystemInfoSync()
    return info.pixelRatio || 2
  } catch (e) {
    return 2
  }
}

module.exports = {
  render,
  drawLine: render,
  drawGauge,
  getDPR,
  fmtClock
}
