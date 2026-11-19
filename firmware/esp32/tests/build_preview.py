"""Build a single-file local preview without network/device access."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
html=(root/'web/index.html').read_text()
css=(root/'web/app.css').read_text()
core=(root/'web/core.mjs').read_text()
app=(root/'web/app.js').read_text()
core=core.replace('export const ','const ').replace('export function ','function ')
import re
app=re.sub(r"^import \{[^\n]+?\} from './core\.mjs';\n?",'',app,count=1,flags=re.MULTILINE)
html=html.replace('<link rel="stylesheet" href="/app.css">','<style>'+css+'</style>')
html=html.replace('<script type="module" src="/app.js"></script>','<script>\n'+core+'\n'+app+'\n</script>')
(root.parent/'brickos_brick_preview.html').write_text(html)
print(root.parent/'brickos_brick_preview.html')
