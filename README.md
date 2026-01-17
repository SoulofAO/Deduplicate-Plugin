<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./media/logo-dark.png">
    <img width="512" height="auto" alt="Project Logo" src="./media/logo-light.png">
</picture>
</p>

---

<div align="center">

[![GitHub](https://img.shields.io/badge/GitHub-blue?style=flat&logo=github)](https://github.com)
[![License](https://img.shields.io/badge/License-purple?style=flat&logo=github)](/LICENSE.md)

</div>

<h1 align="center">
Plugin for Removing Duplicates in Unreal Engine Project
</h1>

<h2 align="center">
    ⚠️ Disclaimer ⚠️
</h2>
<p align="center">
  The author is not responsible for any possible consequences resulting from the use of this project.
  By using the materials in this repository, you automatically agree to the terms of the associated license agreement.
</p>

<details>
  <summary align="center">⚠️ Full Text ⚠️</summary>

1. By using the materials in this repository, you automatically agree to the terms of the associated license agreement.

2. The author provides no warranties, express or implied, regarding the accuracy, completeness, or suitability of this material for any specific purpose.
3. The author shall not be liable for any losses, including but not limited to direct, indirect, incidental, consequential, or special damages, arising from the use or inability to use this material or its accompanying documentation, even if the possibility of such damages was previously communicated.

4. By using this material, you acknowledge and assume all risks associated with its application. Furthermore, you agree that the author cannot be held liable for any issues or consequences arising from its use.

</details>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center">📊 Installation</h1>

<h3 align="left">1. Install the plugin by placing it in the <code>Plugins</code> folder of your Unreal Engine project.</h3>

<h3 align="left">2. Open the plugin main utility window.</h3>

<div align="center">
  <img style="width: 80%; height: auto;" alt="Plugin Main Utility" src="https://github.com/user-attachments/assets/56ca7d7d-54be-448f-bd78-5b2ba71865fd" />
</div>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center">🚀 Quick Start</h1>

<h3 align="left">1. Configure your deduplication algorithm in the window on the right.</h3>

<p align="left">
You configure the algorithm from different <code>DeduplicateObject</code>. <code>DeduplicateObject</code> look for groups of similar objects. These groups are combined into clusters through the sum or multiplication through the <code>Combination Score Method</code>, and then filtered by the <code>Confidence Score</code>.
</p>

<div align="center">
  <img style="width: 80%; height: auto;" alt="Deduplication Algorithm Configuration" src="https://github.com/user-attachments/assets/dbd24ac5-7a01-463f-8a03-b7ecf3dc61b5" />
  <img style="width: 80%; height: auto;" alt="Score Method Configuration" src="https://github.com/user-attachments/assets/cc4ee6d7-2773-46ee-8dbc-7f6121902c03" />
</div>

<h3 align="left">2. Early Check Deduplicate Objects</h3>

<p align="left">
<code>Early Check Deduplicate Objects</code> - separately exist for the pre-evaluation of deduplicated objects, before moving on to their evaluation by heavier algorithms.
</p>

<h3 align="left">3. Analyze and Merge</h3>

<p align="left">
After that, click one of the two <code>Analyze</code> buttons. After work, all the intended duplicators will appear in the Content on the right. Select priority folders or assets, and then click <code>Merge</code>.
</p>

<div align="center">
  <img style="width: 80%; height: auto;" alt="Merge Results" src="https://github.com/user-attachments/assets/6d40c7c5-2a5c-456f-9820-a5bb794a75fc" />
</div>

<h3 align="left">4. Profit.</h3>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center">📊 Working with the Plugin</h1>

<h2 align="center">Algorithm Configuration</h2>

<p align="left">
The deduplication algorithm is configured through multiple <code>DeduplicateObject</code> components. Each <code>DeduplicateObject</code> searches for groups of similar objects within your project.
</p>

<h3 align="left">Combination Score Method</h3>

<p align="left">
Similar object groups are combined into clusters using the <code>Combination Score Method</code>, which can use either sum or multiplication operations to calculate similarity scores.
</p>

<h3 align="left">Confidence Score Filtering</h3>

<p align="left">
The resulting clusters are then filtered by the <code>Confidence Score</code> to ensure only high-probability duplicates are identified for merging.
</p>

<h3 align="left">Early Check Deduplicate Objects</h3>

<p align="left">
<code>Early Check Deduplicate Objects</code> provides a lightweight pre-evaluation step that filters objects before they are processed by the heavier deduplication algorithms. This optimization helps improve performance by reducing the number of objects that need detailed analysis.
</p>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h2 align="center">
> 💡 Additional Information:
</h2>

<p align="left">
For more information, read the attached documentation.
</p>

* * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * *

<h1 align="center"> 📜 License</h1>

<h2 align="center">
  <strong>-------></strong>
  <strong> This project is distributed under the </strong>
  <a href="./LICENSE">License</a>
  <strong><-------</strong>
</h2>

---

<h1 align="center">📬 Feedback</h1>

<p align="center">
If you encounter any issues or have suggestions — create an
<a href="https://github.com/issues">Issue</a>
or
<a href="https://github.com/pulls">Pull Request</a>
</p>

---

<h2 align="center">
📚 Documentation and Showcase
</h2>

<div align="center">

<h3 align="center">Showcase Video</h3>

<p align="center">
  <a href="https://youtu.be/aj4SMnDF4fk?si=Y8tI0uOJMNXPnQGJ">Watch on YouTube</a>
</p>

<h3 align="center">Documentation</h3>

<p align="center">
  <a href="https://docs.google.com/document/d/1CfPzAvMcwM8HJiePUiIiFC_vtNq1W1RfXnq0VOJqHMg/edit?usp=sharing">Read Full Documentation</a>
</p>

</div>
