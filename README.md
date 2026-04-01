# Creo Auto Drawing Plugin

这是一个基于 Creo Toolkit 的 DLL 插件，用于在零件/装配模型界面自动创建同名工程图，并按国标常用的第一角法布置三视图：

- 主视图
- 俯视图（位于主视图下方）
- 左视图（位于主视图右侧）

## 已实现能力

- 识别当前激活模型是否为零件或装配
- 使用当前模型名创建同名 `.drw`
- 用工程图模板建图
- 自动放置三视图
- 自动刷新并保存工程图

## 重要前提

国标图框、标题栏、单位、默认比例等，仍建议放在 **工程图模板** 中统一控制。

本插件负责：

1. 新建同名工程图
2. 关联当前模型
3. 放置三视图

因此请准备一个 **空白但已配置国标图框/格式的模板**。

## 模板配置

插件默认模板直接读取 `config.pro`：

- `template_drawing`

例如：

- `template_drawing D:\ptc\templates\STD_A3_GB.drw`

插件会自动从该路径解析模板模型名并创建工程图。

可选地，你还可以设置基础视图比例：

- `CREO_AUTODRAW_SCALE=0.5`

如果不设置，则使用模板默认比例。

## 编译

1. 使用 Visual Studio 打开工程
2. 选择 **x64** 配置编译
3. 生成结果位于：
   - `x86e_win64/obj/creo_autodraw.dll`

工程默认引用本机 Toolkit 路径：

- `D:\Program Files\PTC\Creo 8.0.9.0\Common Files\protoolkit`

如果你的 Creo 安装路径不同，请修改 `Test.vcxproj` 中的 `CreoToolkitRoot`。

## 加载方式

在 Creo 中通过 Auxiliary Applications / Toolkit 注册 `creo_autodraw.dat`。

`creo_autodraw.dat` 已包含：

- DLL 路径
- TEXT_DIR
- 启动方式

## 使用方式

加载后，在 Creo 顶部菜单中找到：

- `自动出图`
- `一键生成工程图`

点击后即可为当前零件/装配生成同名工程图。

## 说明

- 当前实现按 **国标常用第一角法** 排布三视图
- 若模板不存在、当前对象不是零件/装配，插件会弹出提示
- 若你希望进一步扩展为：
  - 自动标注
  - 自动 BOM
  - 自动明细栏
  - 按零件类别切换 A3/A2/A1 模板
  - 装配自动主视方向识别
  可在此基础上继续扩展
