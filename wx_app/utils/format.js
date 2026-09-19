/**
 * 指标定义与格式化工具
 * 与 backend/app.py 的 THRESHOLD_KEYS / SENSOR_KEYS 及固件 main.c 报警规则保持一致：
 *   Temp >= A_Temp / Humi <= A_Hum / Pre >= A_Pre 报警
 *   MQ2 >= A_MQ2_Value / MQ7 >= A_MQ7_Value 报警，MQ135 <= A_MQ135_Value 报警（值越低空气越差）
 *   GZ_Value >= A_GZ_Value 只触发窗帘，不报警
 * weight > 0 的项参与本地舒适度评分（utils/comfort.js）
 */

/** 网页版信号场配色：数据标记统一青色 #38BDF8，不按指标分色 */
const METRICS = [
  { key: 'Temp',        th: 'A_Temp',        label: '温度',     unit: '°C',  cmp: 'gte', max: 99,   ideal: [22, 26],    color: '#38BDF8', weight: 45, hint: '高于即报警',   thLabel: '报警阈值' },
  { key: 'Humi',       th: 'A_Hum',         label: '湿度',     unit: '%',   cmp: 'lte', max: 99,   ideal: [45, 60],    color: '#38BDF8', weight: 35, hint: '低于即报警',   thLabel: '报警阈值' },
  { key: 'Pre',        th: 'A_Pre',         label: '气压',     unit: 'hPa', cmp: 'gte', max: 2000, ideal: [990, 1025], color: '#38BDF8', weight: 0,  hint: '高于即报警',   thLabel: '报警阈值' },
  { key: 'GZ_Value',   th: 'A_GZ_Value',    label: '光照',     unit: 'ADC', cmp: null,  max: 4095, ideal: [500, 2000], color: '#38BDF8', weight: 20, hint: '高于即开窗帘，不报警', thLabel: '窗帘阈值' },
  { key: 'MQ2_Value',  th: 'A_MQ2_Value',   label: '烟雾',     unit: 'ADC',  cmp: 'gte', max: 4095, ideal: [0, 1200],   color: '#38BDF8', weight: 0,  hint: '高于即报警',   thLabel: '报警阈值' },
  { key: 'MQ7_Value',  th: 'A_MQ7_Value',   label: '一氧化碳', unit: 'ADC',  cmp: 'gte', max: 4095, ideal: [0, 1200],   color: '#38BDF8', weight: 0,  hint: '高于即报警',   thLabel: '报警阈值' },
  { key: 'MQ135_Value',th: 'A_MQ135_Value', label: '空气质量', unit: 'ADC',  cmp: 'lte', max: 4095, ideal: [200, 4095], color: '#38BDF8', weight: 0,  hint: '低于即报警（值越低空气越差）', thLabel: '报警阈值' }
]

/** 参与蜂鸣器报警的指标 */
const ALARM_METRICS = METRICS.filter(m => m.cmp)

/** RGB 灯可控通道（与固件下发属性一致，配色同网页版按钮） */
const RGB = [
  { key: 'R', label: '红 R', color: '#FF6B6B' },
  { key: 'G', label: '绿 G', color: '#34D399' },
  { key: 'B', label: '蓝 B', color: '#38BDF8' }
]

function pad(n) {
  return n < 10 ? '0' + n : '' + n
}

/** 时间戳 -> HH:mm */
function fmtTime(ts) {
  if (!ts) return '--:--'
  const d = new Date(ts)
  return `${pad(d.getHours())}:${pad(d.getMinutes())}`
}

/** 时间戳 -> MM-DD HH:mm:ss */
function fmtDateTime(ts) {
  if (!ts) return '--'
  const d = new Date(ts)
  return `${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

/** 数值格式化，非数字返回 '--' */
function fmtNum(v, digits = 1) {
  const n = Number(v)
  if (v === null || v === undefined || v === '' || isNaN(n)) return '--'
  return digits === 0 ? String(Math.round(n)) : n.toFixed(digits)
}

/** 单项是否越限（与后端 alarm() 相同的规则） */
function isOver(m, data) {
  const v = Number(data[m.key])
  const t = Number(data[m.th])
  if (!m.cmp || isNaN(v) || isNaN(t)) return false
  return m.cmp === 'gte' ? v >= t : v <= t
}

/**
 * 把后端 data（最新属性，含阈值）转成用于展示的指标数组
 * 每项含：当前值、阈值、是否越限、提示文案
 */
function buildMetrics(data) {
  if (!data) return []
  return METRICS.filter(m => {
    const v = data[m.key]
    return v !== null && v !== undefined && v !== ''
  }).map(m => {
    const over = isOver(m, data)
    const t = data[m.th]
    return {
      key: m.key,
      label: m.label,
      unit: m.unit,
      color: m.color,
      value: fmtNum(data[m.key], 0),
      threshold: t === undefined || t === null ? '--' : String(t),
      thresholdLabel: m.thLabel,
      hint: m.hint,
      over: over
    }
  })
}

/** 从 data 取 RGB 灯开关状态 */
function buildRgb(data) {
  if (!data) return []
  return RGB.map(c => ({
    key: c.key,
    label: c.label,
    color: c.color,
    on: data[c.key] === true || data[c.key] === 1 || data[c.key] === '1'
  }))
}

/** 相对时间：刚刚 / x 秒前 / x 分钟前 / x 小时前 */
function ago(ts) {
  if (!ts) return '无数据'
  const s = Math.floor((Date.now() - ts) / 1000)
  if (s < 5) return '刚刚'
  if (s < 60) return `${s} 秒前`
  if (s < 3600) return `${Math.floor(s / 60)} 分钟前`
  return `${Math.floor(s / 3600)} 小时前`
}

module.exports = {
  METRICS,
  ALARM_METRICS,
  RGB,
  fmtTime,
  fmtDateTime,
  fmtNum,
  ago,
  isOver,
  buildMetrics,
  buildRgb
}
