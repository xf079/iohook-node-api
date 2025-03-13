const assert = require('assert');
const path = require('path');
const fs = require('fs');

// 检查构建文件是否存在
try {
  const { ioHook, UiohookKey } = require('../');
  console.log('Import test: PASSED');
  
  // 测试API是否正确
  assert(typeof ioHook.start === 'function', 'ioHook.start should be a function');
  assert(typeof ioHook.stop === 'function', 'ioHook.stop should be a function');
  assert(typeof ioHook.on === 'function', 'ioHook.on should be a function');
  assert(typeof ioHook.off === 'function', 'ioHook.off should be a function');
  console.log('API test: PASSED');
  
  // 测试常量是否存在
  assert(typeof UiohookKey === 'object', 'UiohookKey should be an object');
  assert(UiohookKey.A !== undefined, 'UiohookKey.A should be defined');
  assert(UiohookKey.Escape !== undefined, 'UiohookKey.Escape should be defined');
  console.log('Constants test: PASSED');
  
  console.log('All tests: PASSED');
  process.exit(0);
} catch (error) {
  console.error('Test failed:', error);
  process.exit(1);
} 