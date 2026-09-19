/**
 * 教室详情页 —— 信号场版
 * 页面分节与网页版 frontend/index.html 一一对应：
 *   01 实时状态  hero 大数字 + 状态盒 + 次级读数条 + 报警风险图例   GET /devices
 *   02 仪表      七项指标 Bento（值 / 风险条 / 状态标签）          GET /devices
 *   03 趋势      历史曲线                                        GET /devices/{id}/history
 *   04 控制与阈值 RGB 灯 + 阈值下发                              POST /control · PUT /thresholds
 *   05 AI 分析   环境评估                                        POST /devices/{id}/analyze
 */
const socket = require('../../utils/socket.js')
const api = require('../../utils/api.js')
const fmt = require('../../utils/format.js')

const MAX_POINTS = 1000

/** 阈值表单可选项（与后端 Thresholds 模型一致） */
const TH_FIELDS = fmt.METRICS.map(m => ({
  key: m.th,
  label: m.label,
  max: m.max,
  hint: m.hint
}))

const AI_MINUTES = [30, 60, 180]
const AI_MINUTE_LABELS = ['30 分钟窗口', '1 小时窗口', '3 小时窗口']

/** 报警接近度：1.0 = 恰好在阈值上，与网页版 risk() 相同的归一化 */
function riskOf(m, data) {
  const v = Number(data[m.key])
  const t = Number(data[m.th])
  if (!m.cmp || isNaN(v) || isNaN(t) || !t || !v) return null
  const r = m.cmp === 'gte' ? v / t : t / v
  return Math.max(0, Math.min(1.25, r))
}

const pad2 = n => (n < 10 ? '0' + n : '' + n)

Page({
  data: {
    id: '',
    name: '--',
    online: false,
    lastText: '--',
    agoText: '',
    // 01
    heroTemp: '--',
    heroHumi: '--',
    heroPts: 0,
    stateType: 'off',
    stateTitle: '等待数据',
    stateSub: '正在连接后端',
    stripSensors: [],
    riskRows: [],
    // 02
    cells: [],
    // 03
    tabs: [],
    active: 'Temp',
    activeUnit: '°C',
    activeValue: '--',
    activeRange: '',
    series: [],
    // 04
    rgb: [],
    thFields: TH_FIELDS,
    thValues: {},
    thMsg: null,
    msg: null,
    // 05
    aiMinuteLabels: AI_MINUTE_LABELS,
    aiIndex: 1,
    aiQuestion: '',
    aiLoading: false,
    aiResult: ''
  },

  series: [],
  lastTs: 0,
  lastSeriesSync: 0,
  thDirty: false,

  onLoad(query) {
    this.setData({
      id: decodeURIComponent(query.id || ''),
      thFields: TH_FIELDS,
      aiMinuteLabels: AI_MINUTE_LABELS
    })
    this.loadHistory()
    this.unsubscribe = socket.subscribe(s => this.apply(s))
  },

  onShow() {
    socket.start()
    socket.refresh()
  },

  onPullDownRefresh() {
    this.loadHistory().then(() => wx.stopPullDownRefresh())
    socket.refresh()
  },

  onUnload() {
    if (this.unsubscribe) this.unsubscribe()
  },

  /* --------------------------- 数据 --------------------------- */

  loadHistory() {
    if (!this.data.id) return Promise.resolve()
    return api
      .history(this.data.id, MAX_POINTS)
      .then(rows => {
        this.series = rows || []
        if (this.series.length) {
          this.lastTs = this.series[this.series.length - 1].ts || 0
        }
        this.lastSeriesSync = Date.now()
        this.setData({ series: this.series })
        this.rebuild()
      })
      .catch(() => {})
  },

  apply(s) {
    const device = (s.devices || []).find(d => d.id === this.data.id)
    if (!device) return

    const data = device.data || {}

    // 最新遥测点入列（等价 Web 端 pushPoint）
    if (data.ts && data.ts > this.lastTs) {
      this.lastTs = data.ts
      this.series.push(data)
      if (this.series.length > MAX_POINTS) {
        this.series = this.series.filter((_, i) => i % 2 === 0)
      }
    }

    const tabs = this.buildTabs(data)
    const active = tabs.some(t => t.key === this.data.active)
      ? this.data.active
      : (tabs[0] && tabs[0].key) || 'Temp'
    const activeTab = tabs.find(t => t.key === active) || { unit: '' }

    // 曲线点多，最多 10 秒同步一次视图，避免频繁 setData
    const now = Date.now()
    const syncSeries = !this.lastSeriesSync || now - this.lastSeriesSync > 10000
    if (syncSeries) this.lastSeriesSync = now

    // ---- 01 实时状态 ----
    const stripSensors = fmt.METRICS
      .filter(m => m.key !== 'Temp' && data[m.key] !== undefined && data[m.key] !== null)
      .map(m => ({ key: m.key, label: m.label, unit: m.unit, value: fmt.fmtNum(data[m.key], 0) }))

    const riskRows = fmt.ALARM_METRICS.map(m => {
      const rk = riskOf(m, data)
      const hot = fmt.isOver(m, data)
      return {
        key: m.key,
        label: m.label,
        pct: rk == null ? '—' : Math.round(rk * 100) + '%',
        meterPct: rk == null ? 0 : Math.min(100, Math.round(rk / 1.25 * 100)),
        hot: hot
      }
    })

    // ---- 状态盒（与网页版 renderLive 相同的三态文案）----
    const hits = fmt.ALARM_METRICS.filter(m => fmt.isOver(m, data))
    let stateType = 'ok'
    let stateTitle = '环境正常'
    let stateSub = '六项指标均在阈值内，蜂鸣器静音'
    if (!device.online) {
      stateType = 'off'
      stateTitle = '设备离线'
      stateSub = '超过 30 秒未收到上报，以下为最后一次已知数据'
    } else if (hits.length) {
      stateType = 'bad'
      stateTitle = '环境告警 · 蜂鸣器已响'
      stateSub = '越限：' + hits.map(m => m.label).join('、')
    }

    // ---- 02 仪表 Bento（与网页版 buildBento/renderLive 相同的 chip 逻辑）----
    const cells = fmt.METRICS.map((m, i) => {
      const t = data[m.th]
      const over = fmt.isOver(m, data)
      const rk = riskOf(m, data)
      let chipType = 'dim'
      let chipText = '—'
      let limText = ''
      if (!m.cmp) {
        chipText = '窗帘触发'
        limText = '窗帘阈值 ' + fmt.fmtNum(t, 0)
      } else if (t === undefined || t === null) {
        chipText = '云端未设 · 设备用默认'
      } else if (over) {
        chipType = 'bad'
        chipText = '越限'
        limText = m.hint.replace('，不报警', '') + ' ' + t
      } else {
        chipType = 'ok'
        chipText = '正常'
        limText = m.hint.replace('，不报警', '') + ' ' + t
      }
      return {
        key: m.key,
        idx: pad2(i + 1),
        label: m.label,
        unit: m.unit,
        value: fmt.fmtNum(data[m.key], 0),
        over: over,
        meterPct: rk == null ? 0 : Math.min(100, Math.round(rk / 1.25 * 100)),
        chipType: chipType,
        chipText: chipText,
        limText: limText
      }
    })

    // ---- 03 趋势当前值 / 区间 ----
    const pts = this.series.map(r => Number(r[active])).filter(v => !isNaN(v))
    const activeValue = pts.length ? fmt.fmtNum(pts[pts.length - 1], 0) : '--'
    const activeRange = pts.length
      ? fmt.fmtNum(Math.min.apply(null, pts), 0) + ' – ' + fmt.fmtNum(Math.max.apply(null, pts), 0)
      : ''

    const patch = {
      name: device.name || device.id,
      online: !!device.online,
      lastText: data.ts ? fmt.fmtDateTime(data.ts) : '--',
      agoText: data.ts ? fmt.ago(data.ts) : '',
      heroTemp: fmt.fmtNum(data.Temp, 0),
      heroHumi: fmt.fmtNum(data.Humi, 0) + ' %',
      heroPts: this.series.length,
      stateType: stateType,
      stateTitle: stateTitle,
      stateSub: stateSub,
      stripSensors: stripSensors,
      riskRows: riskRows,
      cells: cells,
      tabs: tabs,
      active: active,
      activeUnit: activeTab.unit,
      activeValue: activeValue,
      activeRange: activeRange,
      rgb: fmt.buildRgb(data)
    }

    if (!this.thDirty) {
      const thValues = {}
      TH_FIELDS.forEach(f => {
        if (data[f.key] !== undefined && data[f.key] !== null) {
          thValues[f.key] = String(data[f.key])
        }
      })
      patch.thValues = thValues
    }

    if (syncSeries) patch.series = this.series
    this.setData(patch)
  },

  /** 趋势指标切换项：设备实际上报过的指标才出现 */
  buildTabs(data) {
    const has = key => {
      if (data[key] !== undefined && data[key] !== null) return true
      return this.series.some(p => p[key] !== undefined && p[key] !== null)
    }
    return fmt.METRICS.filter(m => has(m.key)).map(m => ({
      key: m.key,
      label: m.label,
      unit: m.unit
    }))
  },

  rebuild() {
    this.apply(socket.getState())
  },

  /* --------------------------- 交互 --------------------------- */

  switchMetric(e) {
    const key = e.currentTarget.dataset.key
    const tab = this.data.tabs.find(t => t.key === key)
    const pts = this.series.map(r => Number(r[key])).filter(v => !isNaN(v))
    this.setData({
      active: key,
      activeUnit: tab ? tab.unit : '',
      activeValue: pts.length ? fmt.fmtNum(pts[pts.length - 1], 0) : '--',
      activeRange: pts.length
        ? fmt.fmtNum(Math.min.apply(null, pts), 0) + ' – ' + fmt.fmtNum(Math.max.apply(null, pts), 0)
        : ''
    })
  },

  /* ---- RGB 灯控制（网页版 R/G/B 开关按钮）---- */

  onCtl(e) {
    const key = e.currentTarget.dataset.key
    const on = e.currentTarget.dataset.val === '1'
    const rgb = this.data.rgb.map(c => (c.key === key ? Object.assign({}, c, { on }) : c))
    this.setData({ rgb: rgb, msg: null })

    api
      .control(this.data.id, { [key]: on })
      .then(() => {
        this.setData({ msg: { ok: true, text: `${key} 通道 ${on ? '开' : '关'} 指令已下发` } })
        setTimeout(() => this.setData({ msg: null }), 3000)
      })
      .catch(err => {
        this.setData({ msg: { ok: false, text: `下发失败：${err.message}` } })
        setTimeout(() => this.setData({ msg: null }), 3000)
      })
  },

  /* ---- 阈值下发 ---- */

  onThInput(e) {
    const key = e.currentTarget.dataset.key
    this.thDirty = true
    this.setData({
      thValues: Object.assign({}, this.data.thValues, { [key]: e.detail.value }),
      thMsg: null
    })
  },

  onThReset() {
    this.thDirty = false
    const device = socket.getDevice(this.data.id)
    const data = (device && device.data) || {}
    const thValues = {}
    TH_FIELDS.forEach(f => {
      if (data[f.key] !== undefined && data[f.key] !== null) {
        thValues[f.key] = String(data[f.key])
      }
    })
    this.setData({ thValues: thValues, thMsg: null })
  },

  onThSave() {
    const body = {}
    const errs = []
    TH_FIELDS.forEach(f => {
      const raw = (this.data.thValues[f.key] || '').trim()
      if (raw === '') return // 留空表示不修改
      if (!/^\d+$/.test(raw)) {
        errs.push(`${f.label}：只能填整数`)
        return
      }
      const n = Number(raw)
      if (n < 0 || n > f.max) {
        errs.push(`${f.label}：需在 0 到 ${f.max} 之间`)
        return
      }
      body[f.key] = n
    })

    if (errs.length) {
      this.setData({ thMsg: { ok: false, text: errs.join('；') } })
      return
    }
    if (!Object.keys(body).length) {
      this.setData({ thMsg: { ok: false, text: '没有填写任何阈值' } })
      return
    }

    api
      .setThresholds(this.data.id, body)
      .then(() => {
        this.thDirty = false
        this.setData({ thMsg: { ok: true, text: '阈值已下发，设备实时生效' } })
        socket.refresh()
        setTimeout(() => this.setData({ thMsg: null }), 3000)
      })
      .catch(err => {
        this.setData({ thMsg: { ok: false, text: `下发失败：${err.message}` } })
      })
  },

  /* ---- AI 分析 ---- */

  onAiQuestion(e) {
    this.setData({ aiQuestion: e.detail.value })
  },

  onAiMinute(e) {
    this.setData({ aiIndex: Number(e.detail.value) })
  },

  onAiGo() {
    if (this.data.aiLoading) return
    const minutes = AI_MINUTES[this.data.aiIndex] || 60
    this.setData({ aiLoading: true, aiResult: '' })

    api
      .analyze(this.data.id, {
        minutes: minutes,
        question: (this.data.aiQuestion || '').trim()
      })
      .then(res => {
        this.setData({ aiResult: (res && res.analysis) || '（无输出）' })
      })
      .catch(err => {
        this.setData({ aiResult: `分析失败：${err.message}` })
      })
      .then(() => this.setData({ aiLoading: false }))
  }
})
