Plugin for removing duplicates in the Unreal Engine project.

Quick start:
Install the plugin by placing it in the Plugins folder in the project.
Open the plugin main utlity.

<img width="495" height="856" alt="image" src="https://github.com/user-attachments/assets/56ca7d7d-54be-448f-bd78-5b2ba71865fd" />

Configure your deduplication algorithm in the window on the right. 
Configure your deduplication algorithm in the window on the right. 
You configure the algorithm from different DeduplicateObject. DeduplicateObject look for groups of similar objects. These groups are combined into clusters through the sum or multiplication through the COmbination Score Method, and then filtered by the Confediancle Score. 

Early Check Deduplicate Objects - separately exist for the pre-evaluation of deduplicated objects, before moving on to their evaluation by heavier algorithms.

<img width="504" height="615" alt="image" src="https://github.com/user-attachments/assets/dbd24ac5-7a01-463f-8a03-b7ecf3dc61b5" />
<img width="512" height="213" alt="image" src="https://github.com/user-attachments/assets/cc4ee6d7-2773-46ee-8dbc-7f6121902c03" />

After that, click one of the two Analyze. After work, all the intended duplicators will appear in the Content on the right. Select priority folders or assets, and then click Merge. 

<img width="321" height="246" alt="image" src="https://github.com/user-attachments/assets/6d40c7c5-2a5c-456f-9820-a5bb794a75fc" />

Profit. 
I will publish more information later.

Copyright / Usage Rules — Deduplication
Deduplication is released as Open Source.

Any derivative, fork, or plugin whose primary purpose is finding/removing/merging duplicate assets for Unreal Engine must be open source and distributed under compatible terms.

Projects that reuse the code for a different primary purpose may be closed/commercial, but must include clear attribution (see example).

Do not remove or alter the original LICENSE, NOTICE files or existing author headers in source files.

The code is provided “AS IS”; authors give no warranties and accept no liability for damages.

Attribution example (place in README/About/Docs):
Based on "Deduplication" — https://github.com/your-repo/deduplication

The plugin author(s) reserve the right to modify these rules.
