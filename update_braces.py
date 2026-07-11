import re

def update_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # The block to find and replace
    old_code = r'''  while \(packed != -1\) \{
      int markerValue = packed & 0xFF;'''

    new_code = '''  while (packed != -1)
  {
      int markerValue = packed & 0xFF;'''

    content = re.sub(old_code, new_code, content)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

update_file('D:/Prog/imwrap-v6/docs/imwrap_manual_fr/FR_08_reference_api_ags.md')
update_file('D:/Prog/imwrap-v6/docs/imwrap_manual_en/EN_08_ags_api_reference.md')
update_file('D:/Prog/imwrap-v6-wiki/FR_08_reference_api_ags.md')
update_file('D:/Prog/imwrap-v6-wiki/EN_08_ags_api_reference.md')
