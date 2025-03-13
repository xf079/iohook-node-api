import { ioHook, UiohookKey } from './index'

// 监听键盘按下事件
ioHook.on('keydown', (e) => {
  console.log('键盘按下:', e.keycode)
  
  // 按下Q键时输出Hello
  if (e.keycode === UiohookKey.Q) {
    console.log('Hello!')
  }
  
  // 按下Escape键时退出程序
  if (e.keycode === UiohookKey.Escape) {
    console.log('退出程序')
    ioHook.stop()
    process.exit(0)
  }
})

// 监听鼠标移动事件
ioHook.on('mousemove', (e) => {
  console.log(`鼠标位置: x=${e.x}, y=${e.y}`)
})

// 监听鼠标点击事件
ioHook.on('click', (e) => {
  console.log(`鼠标点击: 按钮=${e.button}, x=${e.x}, y=${e.y}`)
})

// 监听鼠标滚轮事件
ioHook.on('wheel', (e) => {
  console.log(`鼠标滚轮: 方向=${e.direction}, 旋转=${e.rotation}`)
})

// 启动钩子
console.log('启动全局钩子，按Escape键退出')
ioHook.start()

// 注册进程退出时的清理函数
process.on('exit', () => {
  ioHook.stop()
}) 