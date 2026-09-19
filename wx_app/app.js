const config = require('./utils/config.js')
const socket = require('./utils/socket.js')

App({
  globalData: {
    // 后端地址统一在 utils/config.js 里配置，改地址只需改那一处
    baseUrl: config.BASE_URL
  },

  onLaunch() {
    console.log('[app] 后端地址 =', config.BASE_URL)
    socket.start()
  },

  onShow() {
    socket.start()
  }
})
