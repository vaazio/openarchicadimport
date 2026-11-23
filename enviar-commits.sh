#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

uso() {
    echo "Uso: $0 [URL-do-repositorio]"
    echo "Informe a URL no primeiro envio para configurar o remoto origin."
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    uso
    exit 0
fi
if (( $# > 1 )); then
    uso >&2
    exit 2
fi

if ! branch=$(git symbolic-ref --quiet --short HEAD); then
    echo "Selecione uma branch com git switch antes de enviar." >&2
    exit 1
fi

url=${1:-}
if destino=$(git remote get-url --push origin 2>/dev/null); then
    if [[ -n $url && $url != "$destino" ]]; then
        echo "A URL informada difere do destino configurado. Confira com git remote -v." >&2
        exit 2
    fi
elif [[ -n $url ]]; then
    git remote add -- origin "$url"
else
    uso >&2
    exit 2
fi

git push --set-upstream origin "refs/heads/$branch:refs/heads/$branch"
