<p align="center">
  <strong>-------></strong>
  <a href="/README.md">English</a> |
  <a href="/README.ru.md">Русский</a> |
  <a href="/README.zh.md">中文</a>
  <strong><-------</strong>
</p>

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./media/logo-dark.png">
    <img width="512" height="auto" alt="项目标志" src="./media/logo-light.png">
</picture>
</p>

---

<div align="center">

[![GitHub](https://img.shields.io/badge/GitHub-blue?style=flat&logo=github)](https://github.com)
[![License](https://img.shields.io/badge/License-purple?style=flat&logo=github)](/LICENSE.md)

</div>

<h1 align="center">
用于 Unreal Engine 项目中删除重复项的插件
</h1>

<h2 align="center">
    ⚠️ 免责声明 ⚠️
</h2>
<p align="center">
  作者不对因使用本项目而产生的任何可能后果负责。
  使用此存储库中的材料，即表示您自动同意相关许可协议的条款。
</p>

<details>
  <summary align="center">⚠️ 完整文本 ⚠️</summary>

1. 使用此存储库中的材料，即表示您自动同意相关许可协议的条款。

2. 作者不对此材料的准确性、完整性或适合任何特定用途的适用性提供任何明示或暗示的保证。
3. 对于因使用或无法使用此材料或其随附文档而产生的任何损失，包括但不限于直接、间接、偶然、后果性或特殊损害，即使之前已告知此类损害的可能性，作者也不承担责任。

4. 使用此材料，您承认并承担与其应用相关的所有风险。此外，您同意作者不对因使用而产生的任何问题或后果承担责任。

</details>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center">📊 安装</h1>

<h3 align="left">1. 通过将插件放置在 Unreal Engine 项目的 <code>Plugins</code> 文件夹中来安装插件。</h3>

<h3 align="left">2. 打开插件主实用程序窗口。</h3>

<div align="center">
  <img style="width: 80%; height: auto;" alt="插件主实用程序" src="./media/plugin-main-utility.png" />
</div>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center">🚀 快速开始</h1>

<h3 align="left">1. 在右侧窗口中配置您的去重算法。</h3>

<p align="left">
您可以从不同的 <code>DeduplicateObject</code> 配置算法。<code>DeduplicateObject</code> 查找相似对象的组。这些组通过 <code>Combination Score Method</code> 通过求和或乘法组合成集群，然后通过 <code>Confidence Score</code> 和 <code>Group Confidence Score</code> 进行过滤。
</p>

<div align="center">
  <img style="width: 80%; height: auto;" alt="去重算法配置" src="./media/deduplication-algorithm-configuration.png" />
  <img style="width: 80%; height: auto;" alt="评分方法配置" src="./media/score-method-configuration.png" />
</div>

<h3 align="left">2. 分析</h3>
<p align="left">
  从控制面板选择 Analyze，或选择要分析值的文件夹，然后单击 Analyze in Folder。 
</p>
<p align="left">
等待分析操作完成。
</p>
<h3 align="left">3. 合并</h3>
<p align="left">
完成后，所有潜在的重复对象将出现在右侧的"Content"部分。您可以通过选择特定资源或文件夹在统计窗口中查看合并参数和测试结果，并配置过滤器。选择作为去重主要实体的优先文件夹或资源，然后单击 <code>Unite</code>。
</p>

<div align="center">
  <img style="width: 80%; height: auto;" alt="合并结果" src="./media/merge-results.png" />
</div>

<h3 align="left">4. 完成。有关更具体的信息，请阅读文档</h3>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center"> 📜 许可证</h1>

<h2 align="center">
  <strong>-------></strong>
  <strong> 本项目根据 </strong>
  <a href="./LICENSE">许可证</a>
  <strong> 分发</strong>
  <strong><-------</strong>
</h2>

---

<h1 align="center">📬 反馈</h1>

<p align="center">
如果您遇到任何问题或有建议 — 请创建
<a href="https://github.com/issues">Issue</a>
或
<a href="https://github.com/pulls">Pull Request</a>
</p>

---

<h2 align="center">
📚 文档和展示
</h2>

<div align="center">

<h3 align="center">展示视频</h3>

<p align="center">
  <a href="https://youtu.be/aj4SMnDF4fk?si=Y8tI0uOJMNXPnQGJ">在 YouTube 上观看演示</a>
</p>

<h3 align="center">文档</h3>

<p align="center">
  <a href="https://docs.google.com/document/d/1CfPzAvMcwM8HJiePUiIiFC_vtNq1W1RfXnq0VOJqHMg/edit?usp=sharing">阅读完整文档</a>
</p>

</div>
