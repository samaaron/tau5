/*!-----------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Version: 0.52.0(f6dc0eb8fce67e57f6036f4769d92c1666cdf546)
 * Released under the MIT license
 * https://github.com/microsoft/monaco-editor/blob/main/LICENSE.txt
 *-----------------------------------------------------------------------------*/


// src/basic-languages/mysql/mysql.contribution.ts
import { registerLanguage } from "../_.contribution.js";
registerLanguage({
  id: "mysql",
  extensions: [],
  aliases: ["MySQL", "mysql"],
  loader: () => {
    if (false) {
      return new Promise((resolve, reject) => {
        __require(["vs/basic-languages/mysql/mysql"], resolve, reject);
      });
    } else {
      return import("./mysql.js");
    }
  }
});
