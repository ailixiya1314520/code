/**
 * 舒适度评级（本地计算，后端无此接口）
 * 新后端 data 只有传感器值与阈值，舒适区间在此维护：
 *   - 单项得分：落在舒适区间得 100 分，越界后线性衰减（最低 0 分）
 *   - 综合得分：按 METRICS 的 weight 加权平均（weight=0 的项不参与）
 *   - 等级：>=85 优 / >=70 良 / >=55 一般 / 其它 差
 */
const { METRICS } = require('./format.js')

function toNum(v) {
  if (v === null || v === undefined || v === '') return NaN
  return Number(v)
}

function clamp(v, lo, hi) {
  return Math.min(hi, Math.max(lo, v))
}

/** 单个指标得分 0~100（越界后按区间外 1/3 范围线性衰减到 0） */
function scoreOf(v, m) {
  const [lo, hi] = m.ideal
  if (v >= lo && v <= hi) return 100
  if (v < lo) {
    const span = Math.max(lo / 3, 1)
    return clamp(100 - ((lo - v) / span) * 100, 0, 100)
  }
  const span = Math.max((hi - lo) / 3, 1)
  return clamp(100 - ((v - hi) / span) * 100, 0, 100)
}

/**
 * @param {object} data 设备最新属性（data 字段）
 * @returns {{score:number, level:string, color:string, tips:string[], detail:Array}}
 */
function evaluate(data) {
  data = data || {}
  const detail = []
  const tips = []
  let weighted = 0
  let totalWeight = 0

  METRICS.forEach(m => {
    const v = toNum(data[m.key])
    if (isNaN(v)) return

    const score = Math.round(scoreOf(v, m))
    detail.push({
      key: m.key,
      label: m.label,
      unit: m.unit,
      color: m.color,
      value: v,
      score: score
    })

    if (m.weight > 0) {
      weighted += score * m.weight
      totalWeight += m.weight

      if (v < m.ideal[0]) tips.push(`${m.label}偏低（${fmt(v)}${m.unit}），建议调节`)
      else if (v > m.ideal[1]) tips.push(`${m.label}偏高（${fmt(v)}${m.unit}），建议通风或遮阳`)
    }
  })

  const score = totalWeight > 0 ? Math.round(weighted / totalWeight) : 0
  let level = '差'
  let color = '#ff5c6c'
  if (score >= 85) { level = '优'; color = '#00e6a8' }
  else if (score >= 70) { level = '良'; color = '#3ba1ff' }
  else if (score >= 55) { level = '一般'; color = '#ffb63b' }

  if (!tips.length) tips.push('各项指标处于舒适区间，无需调节')

  return { score: score, level: level, color: color, tips: tips, detail: detail }
}

function fmt(v) {
  return Math.abs(v - Math.round(v)) < 1e-6 ? String(Math.round(v)) : String(v)
}

module.exports = { evaluate, scoreOf }
