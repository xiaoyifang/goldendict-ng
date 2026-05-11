import sys
import os
import xml.etree.ElementTree as ET

def update_metainfo(file_path, version, date, repo, tag_name):
    """
    Updates the AppStream metainfo XML file using semantic XML parsing.
    Handles nested HTML tags correctly and prunes old releases.
    """
    if not os.path.exists(file_path):
        print(f"Error: File {file_path} not found.")
        sys.exit(1)

    # Use a parser that preserves comments (Python 3.8+)
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    try:
        tree = ET.parse(file_path, parser=parser)
    except ET.ParseError as e:
        print(f"Error parsing XML: {e}")
        sys.exit(1)

    root = tree.getroot()
    
    # Find the <releases> tag
    releases = root.find('releases')
    if releases is None:
        releases = ET.SubElement(root, 'releases')

    # Check if version already exists
    for release in releases.findall('release'):
        if release.get('version') == version:
            print(f"Version {version} already exists. Skipping update.")
            return False

    # Create new <release> element
    new_release = ET.Element('release', version=version, date=date)
    description = ET.SubElement(new_release, 'description')
    
    # Build description content from changelog.txt
    has_changelog = False
    if os.path.exists('changelog.txt'):
        with open('changelog.txt', 'r', encoding='utf-8-sig') as f:
            cl_lines = [l.strip() for l in f.readlines() if l.strip() and not l.startswith('#') and 'Based on branch' not in l]
            
            if cl_lines:
                has_changelog = True
                ul = ET.SubElement(description, 'ul')
                for item in cl_lines[:20]:
                    clean_item = item.lstrip('-* ').strip()
                    li = ET.SubElement(ul, 'li')
                    li.text = clean_item
                
                if len(cl_lines) > 20:
                    li_more = ET.SubElement(ul, 'li')
                    li_more.text = 'Check the '
                    a = ET.SubElement(li_more, 'a')
                    a.set('href', f'https://github.com/{repo}/releases/tag/{tag_name}')
                    a.text = 'Full Release Notes'
                    a.tail = ' for more details.'

    if not has_changelog:
        p = ET.SubElement(description, 'p')
        p.text = f"Stable release {version}"

    # Insert at the beginning of <releases>
    releases.insert(0, new_release)

    # Prune to 5 records
    all_releases = releases.findall('release')
    if len(all_releases) > 5:
        for old_rel in all_releases[5:]:
            releases.remove(old_rel)

    # Indent for pretty printing (Python 3.9+)
    if hasattr(ET, 'indent'):
        ET.indent(root, space="  ", level=0)

    # Write back with XML declaration and correct encoding
    tree.write(file_path, encoding='utf-8', xml_declaration=True)
    print(f"Successfully updated {file_path} for version {version}.")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 6:
        print("Usage: python update_metainfo.py <file_path> <version> <date> <repo> <tag_name>")
        sys.exit(1)
    
    update_metainfo(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    sys.exit(0)
