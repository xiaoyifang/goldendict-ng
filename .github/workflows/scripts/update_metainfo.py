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
    ul = ET.SubElement(description, 'ul')
    
    li_more = ET.SubElement(ul, 'li')
    li_more.text = 'Check the full release notes for more details.'
    
    url = ET.SubElement(new_release, 'url')
    url.set('type', 'details')
    url.text = f'https://github.com/{repo}/releases/tag/{tag_name}'

    # Insert at the beginning of <releases>
    releases.insert(0, new_release)

    # Prune to 5 records and ensure only the first one has a description
    all_releases = releases.findall('release')
    for i, rel in enumerate(all_releases):
        if i > 0:
            desc = rel.find('description')
            if desc is not None:
                rel.remove(desc)
        if i >= 5:
            releases.remove(rel)

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
