import { build } from "esbuild";
import { readFile, writeFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";

const raiz = fileURLToPath(new URL(".", import.meta.url));
const pacote = await build({absWorkingDir:raiz, entryPoints:["janela.js"], bundle:true,
  minify:true, write:false, format:"iife", target:"chrome105", legalComments:"inline"});
const modelo = await readFile(new URL("./janela.html",import.meta.url),"utf8");
await writeFile(process.argv[2], modelo.replace("<!-- script -->", () =>
  "<script>"+pacote.outputFiles[0].text.replace(/<\/script/gi,"<\\/script")+"</script>"));

