// 访问令牌：网关开鉴权后（--auth-token）所有 /api 与 WS 都要带。
// 存 localStorage，401 时派发 sbs-unauthorized 事件，App.vue 弹窗收令牌。
//
// ⚠️ Safari 对 http://局域网IP / 隐私模式下访问 localStorage 会抛
//   SecurityError: The operation is insecure——**模块加载时**裸调会把整页打成白屏
//   （2026-09-28 手机/桌面 Safari 白屏事故根因）。必须走安全包装 + 内存回退。
const TOKEN_KEY = 'sbs_token'
let unauthorizedEmittedFor = null
const memStore = new Map()   // localStorage 不可用时的回退（本次会话内有效）

function safeGet(key) {
  try {
    return window.localStorage.getItem(key)
  } catch {
    return memStore.get(key) ?? null
  }
}
function safeSet(key, value) {
  try {
    window.localStorage.setItem(key, value)
  } catch {
    memStore.set(key, value)
  }
}

export function getToken() {
  return safeGet(TOKEN_KEY) || ''
}
export function setToken(t) {
  safeSet(TOKEN_KEY, t.trim())
  unauthorizedEmittedFor = null
}

function onUnauthorized() {
  // 同一个 token 只提醒一次，避免页面并发请求叠出多个弹窗
  const cur = getToken()
  if (cur === unauthorizedEmittedFor) return
  unauthorizedEmittedFor = cur
  window.dispatchEvent(new CustomEvent('sbs-unauthorized'))
}

async function get(url, params) {
  const qs = params ? '?' + new URLSearchParams(params) : ''
  const res = await fetch('/api' + url + qs, { headers: authHeaders() })
  if (!res.ok) {
    if (res.status === 401) onUnauthorized()
    let msg = res.statusText
    try { msg = (await res.json()).error || msg } catch { /* keep */ }
    throw new Error(msg)
  }
  return res.json()
}

async function post(url) {
  const res = await fetch('/api' + url, { method: 'POST', headers: authHeaders() })
  if (!res.ok) {
    if (res.status === 401) onUnauthorized()
    let msg = res.statusText
    try { msg = (await res.json()).error || msg } catch { /* keep */ }
    throw new Error(msg)
  }
  return res.json()
}

async function postJson(url, body) {
  const res = await fetch('/api' + url, {
    method: 'POST',
    headers: { ...authHeaders(), 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
  if (!res.ok) {
    if (res.status === 401) onUnauthorized()
    let msg = res.statusText
    try { msg = (await res.json()).error || msg } catch { /* keep */ }
    throw new Error(msg)
  }
  return res.json()
}

function authHeaders() {
  const t = getToken()
  return t ? { Authorization: 'Bearer ' + t } : {}
}

/**
 * 对战 WS 封装：onEvent(type, data) 分发网关转发的服务端帧。
 * send* 方法对应浏览器→网关的消息。
 */
class BattleSocket {
  constructor(onEvent, onClosed) {
    this.onEvent = onEvent
    this.onClosed = onClosed
    this.ws = null
    this._pending = []
  }

  connect() {
    const proto = location.protocol === 'https:' ? 'wss' : 'ws'
    // WS 握手带不了自定义头，token 走 query
    const q = getToken() ? '?token=' + encodeURIComponent(getToken()) : ''
    this.ws = new WebSocket(`${proto}://${location.host}/ws/battle${q}`)
    this.ws.onopen = () => {
      // CONNECTING 期间积压的消息（connect 后立刻 init）在这里补发
      for (const msg of this._pending) this.ws.send(msg)
      this._pending = []
    }
    this.ws.onmessage = (e) => {
      try {
        const { type, data, dataHex } = JSON.parse(e.data)
        this.onEvent(type, data ?? { hex: dataHex })
      } catch { /* 忽略坏帧 */ }
    }
    this.ws.onclose = () => this.onClosed && this.onClosed()
  }

  _send(obj) {
    if (this.ws?.readyState === 1) {
      this.ws.send(JSON.stringify(obj))
    } else {
      this._pending.push(JSON.stringify(obj))
    }
  }
  init(lineup) { this._send({ type: 'init', lineup }) }
  action(robot, actionType, index) { this._send({ type: 'action', robot, actionType, index }) }
  sync() { this._send({ type: 'sync' }) }
  fullstate() { this._send({ type: 'fullstate' }) }
  legal(player) { this._send({ type: 'legal', player }) }
  step() { this._send({ type: 'step' }) }
  continue() { this._send({ type: 'continue' }) }
  breakpoint(state) { this._send({ type: 'breakpoint', state }) }
  // 调试手术（二期）：ops = [{op:'set_hp', side, slot, value}, ...] 白名单操作集
  mutate(ops) { this._send({ type: 'mutate', ops_payload: { ops } }) }
  close() { this.ws?.close() }
}

export { get, post, postJson, BattleSocket }
