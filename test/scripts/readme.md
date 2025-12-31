# pytest命令行参数
-s: 显示输出调试信息,包括print打印的信息
-v: 显示更详细的信息
-n:支持多线程运行脚本（需要保持用例彼此独立）
--reruns NUM:失败用例重跑次数
-x:表示只要有一个用例报错，那么测试停止
-k:模糊匹配字符串进行用例跑测

# 目录结构
```
/hitrace/test/scripts
├─reports # 测试报告目录
├─testModule
|   ├─test_hitrace_cmd.py # 测试资源文件，存放测试过程中使用到的文件
├─main.py # 测试用例执行入口
├─pytest.ini # pytest配置文件
└─requirements.txt # 依赖文件
```

## 测试用例执行
windows环境下执行测试用例:
进入scripts目录
### 方式一：
```
    python main.py
```

## 测试报告
执行python main.py后,会在reports目录下生成测试报告