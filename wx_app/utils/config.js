/**
 * 全局配置 —— 后端地址的唯一来源
 * app.js 与 utils/api.js 都从这里取，避免多处硬编码改漏一处。
 *
 * 对应 backend/app.py（FastAPI，Classroom Monitor API），
 * 接口文档见 {BASE_URL}/docs。
 *
 * ⚠️ 手机上跑小程序时，localhost 指的是手机自己：
 *    开发者工具模拟器可用 http://localhost:8000，
 *    真机必须填电脑局域网 IP；正式上线需 https + 已备案域名。
 * 换地址只改下面这一行即可（结尾不要带 '/'）。
 */
module.exports = {
  BASE_URL: 'http://43.251.117.109:6677'
}
