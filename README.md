# [ai_history](https://github.com/yichun822/ai_history)的stm32移植版

**NOTE:**  
-
- 这是一个长期项目，工程量还蛮大的，说是移植，其实和重写没啥区别了
- 我没有wifi和语音模块，所以需要协助

**架构介绍** 
-
- Lib\ ... ：存外部库
- Core\:
  - message: 网络处理 **需要协助!!!**
  - user_interaction: 获取并解析用户输入 **需要协助**
  - virtual_screen: 把消息解析并存入虚拟显存
  - shader: 渲染逻辑
  - control: 由于是裸机直接写的（也不需要rtos）所以把中断、主循环和主状态机写这里，便于查找
  - history_object: 解析时临时结构