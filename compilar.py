"""Prepara uma cópia do template oficial e compila o add-on no Windows."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

RAIZ = Path(__file__).resolve().parent


def executar(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def preparar(template, projeto, mdid):
    shutil.copytree(template, projeto, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns(".git", "Build", "RINT"))
    for pasta in ("Src", "RINT", "interface"):
        shutil.copytree(RAIZ / pasta, projeto / pasta, dirs_exist_ok=True)
    npm = shutil.which("npm.cmd")
    if not npm:
        raise SystemExit("Instale Node.js 22+ com npm para compilar a interface.")
    ui = RAIZ / "previa"
    subprocess.run([npm, "ci", "--prefix", str(ui), "--no-audit", "--no-fund"], check=True)
    executar("node", ui / "empacotar.mjs", projeto / "interface" / "previa.html")
    (projeto / "interface" / "selecao.rc").touch()
    config = projeto / "config.json"
    dados = json.loads(config.read_text(encoding="utf-8-sig"))
    dados.update(addOnName="OPENARCHICADIMPORT", version="0.2.0", languages=["INT"],
                 defaultLanguage="INT", description="OPENARCHICADIMPORT",
                 copyright={"name": "OPENARCHICADIMPORT", "year": "%Y"})
    config.write_text(json.dumps(dados, indent=2), encoding="utf-8")
    if mdid:
        arquivo = projeto / "RFIX" / "AddOnFix.grc"
        conteudo = arquivo.read_text(encoding="utf-8-sig")
        inicio = conteudo.index("{", conteudo.index("'MDID'"))
        fim = conteudo.index("}", inicio)
        arquivo.write_text(conteudo[:inicio+1] + f"\n {mdid[0]}\n {mdid[1]}\n" +
                           conteudo[fim:], encoding="utf-8")
    with (projeto / "CMakeLists.txt").open("a", encoding="utf-8") as cmake:
        cmake.write("\n" + (RAIZ / "dependencias.cmake").read_text(encoding="utf-8"))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("sdk", type=Path, help="Pasta Support do Development Kit correspondente")
    p.add_argument("--versao", type=int, choices=(28, 29, 30), default=30)
    p.add_argument("--mdid", nargs=2, type=int, metavar=("DESENVOLVEDOR", "ADDON"))
    args = p.parse_args()
    sdk = args.sdk.resolve()
    if not (sdk / "Inc" / "ACAPinc.h").is_file():
        p.error("Informe a pasta Support que contém Inc/ACAPinc.h")
    trabalho = RAIZ / "build"
    trabalho.mkdir(exist_ok=True)
    template = trabalho / "template"
    if not template.exists():
        executar("git", "clone", "--recurse-submodules",
                 "https://github.com/GRAPHISOFT/archicad-addon-cmake.git", template)
    executar("git", "-C", template, "submodule", "update", "--init", "--recursive")
    projeto = trabalho / f"projeto-{args.versao}"
    preparar(template, projeto, args.mdid)
    saida = trabalho / f"windows-{args.versao}"
    # O compilador de recursos do SDK inclui todos os .rc2 presentes nesta pasta.
    recursos = saida / "ResourceObjects"
    anterior = recursos / "Importador3D.gen.res.rc2"
    if anterior.exists():
        anterior.unlink()
        (recursos / "AddOnResources.stamp").unlink(missing_ok=True)
    executar("cmake", "-S", projeto, "-B", saida, "-G", "Visual Studio 17 2022",
             "-A", "x64", f"-DAC_VERSION={args.versao}", f"-DAC_API_DEVKIT_DIR={sdk}",
             "-DAC_WIN_LANGCHARSET=040904b0", "-DAC_WIN_LANGUAGEID=1033", "-DAC_WIN_CHARSETID=1200",
             f"-DLP_XML_CONVERTER_FOLDER={sdk / 'LP_XMLConverter'}")
    executar("cmake", "--build", saida, "--config", "Release", "--target", "CMakeTarget")
    arquivo = saida / "Release" / "OPENARCHICADIMPORT.apx"
    if not arquivo.is_file():
        raise SystemExit(f"Build terminou sem produzir {arquivo}.")
    print(arquivo)


if __name__ == "__main__":
    main()
