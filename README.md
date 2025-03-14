# iohook-node-api

Node.js N-API绑定库，用于libuiohook，提供全局键盘和鼠标钩子功能。本项目基于uiohook-napi库，但包含自己的libuiohook实现，应用了关键补丁和改进。

## 功能特点

- 全局监听键盘事件（按键按下、释放）
- 全局监听鼠标事件（移动、点击、按下、释放、滚轮）
- 支持模拟键盘输入
- 跨平台支持（Windows、macOS、Linux）
- 使用N-API实现，提供更好的ABI稳定性
- 包含经过修改的libuiohook库，改进了UIPI处理和键盘行为

## 安装

```bash
npm install iohook-node-api
```

## 使用示例

```javascript
const { ioHook, UiohookKey } = require('iohook-node-api');

// 监听键盘按下事件
ioHook.on('keydown', (e) => {
  console.log('键盘按下:', e.keycode);
  
  // 按下Q键时输出Hello
  if (e.keycode === UiohookKey.Q) {
    console.log('Hello!');
  }
  
  // 按下Escape键时退出程序
  if (e.keycode === UiohookKey.Escape) {
    console.log('退出程序');
    ioHook.stop();
    process.exit(0);
  }
});

// 监听鼠标移动事件
ioHook.on('mousemove', (e) => {
  console.log(`鼠标位置: x=${e.x}, y=${e.y}`);
});

// 监听鼠标点击事件
ioHook.on('click', (e) => {
  console.log(`鼠标点击: 按钮=${e.button}, x=${e.x}, y=${e.y}`);
});

// 监听鼠标滚轮事件
ioHook.on('wheel', (e) => {
  console.log(`鼠标滚轮: 方向=${e.direction}, 旋转=${e.rotation}`);
});

// 启动钩子
console.log('启动全局钩子，按Escape键退出');
ioHook.start();

// 注册进程退出时的清理函数
process.on('exit', () => {
  ioHook.stop();
});
```

## API

### 事件

- `keydown` - 键盘按键按下
- `keyup` - 键盘按键释放
- `mousedown` - 鼠标按钮按下
- `mouseup` - 鼠标按钮释放
- `mousemove` - 鼠标移动
- `click` - 鼠标点击
- `wheel` - 鼠标滚轮滚动
- `input` - 所有输入事件

### 方法

- `ioHook.start()` - 启动钩子
- `ioHook.stop()` - 停止钩子
- `ioHook.keyTap(keycode, [modifiers])` - 模拟按键点击
- `ioHook.keyToggle(keycode, 'down'|'up')` - 模拟按键按下或释放

### 键码常量

库提供了`UiohookKey`对象，包含所有键盘按键的键码常量，例如：

```javascript
UiohookKey.A          // 字母A键
UiohookKey.Escape     // Escape键
UiohookKey.Space      // 空格键
UiohookKey.ArrowUp    // 上箭头键
UiohookKey.F1         // F1功能键
UiohookKey.Ctrl       // 左Ctrl键
UiohookKey.Alt        // 左Alt键
UiohookKey.Shift      // 左Shift键
UiohookKey.Meta       // 左Meta键（Windows键/Command键）
```

## 内部改进

相比于原始的libuiohook和uiohook-napi，本项目包含了以下关键改进：

1. 修复了Windows下扩展键的处理逻辑
2. 改用GetAsyncKeyState替代GetKeyState获取按键状态
3. 增加了对UIPI (User Interface Privilege Isolation)的处理
4. 改进了前台窗口变化时的处理逻辑
5. 修改了X11平台下的一些包含条件

## 许可证

MIT
