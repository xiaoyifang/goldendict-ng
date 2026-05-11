import sys
import os
import html

def update_metainfo(file_path, version, date, repo, tag_name):
    """
    Updates the AppStream metainfo XML file with a new release record.
    Limits the number of release records to 5 and the description to 20 lines.
    """
    if not os.path.exists(file_path):
        print(f"Error: File {file_path} not found.")
        sys.exit(1)

    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Check if this version already exists
    if any(f'version="{version}"' in line for line in lines):
        print(f"Version {version} already exists in {file_path}. Skipping update.")
        return False

    # Build list-based description from changelog.txt
    desc_html = f"<p>Stable release {version}</p>"
    if os.path.exists('changelog.txt'):
        with open('changelog.txt', 'r') as f:
            # Filter meaningful lines (exclude headers and branch info)
            cl_lines = [l.strip() for l in f.readlines() if l.strip() and not l.startswith('#') and 'Based on branch' not in l]
            if cl_lines:
                desc_html = "<ul>\n"
                # Limit to first 20 entries
                for item in cl_lines[:20]:
                    clean_item = item.lstrip('-* ').strip()
                    desc_html += f"          <li>{html.escape(clean_item)}</li>\n"
                
                if len(cl_lines) > 20:
                    desc_html += f"          <li>Check the <a href=\"https://github.com/{repo}/releases/tag/{tag_name}\">Full Release Notes</a> for more details.</li>\n"
                
                desc_html += "        </ul>"

    new_lines = []
    added = False
    for line in lines:
        new_lines.append(line)
        if '<releases>' in line and not added:
            # Insert the new release record right after the <releases> tag
            new_lines.append(f'    <release version="{version}" date="{date}">\n')
            new_lines.append(f'      <description>\n')
            new_lines.append(f'        {desc_html}\n')
            new_lines.append(f'      </description>\n')
            new_lines.append(f'    </release>\n')
            added = True

    # Pruning: Keep only the most recent 5 records
    final_lines = []
    release_count = 0
    in_releases = False
    skip_mode = False
    
    for line in new_lines:
        if '<releases>' in line:
            in_releases = True
            final_lines.append(line)
            continue
        if '</releases>' in line:
            in_releases = False
            final_lines.append(line)
            continue
            
        if in_releases:
            if '<release' in line:
                release_count += 1
                if release_count > 5:
                    skip_mode = True
                else:
                    skip_mode = False
            
            if not skip_mode:
                final_lines.append(line)
        else:
            final_lines.append(line)

    # Write the updated content back to the file
    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(final_lines)
    
    print(f"Successfully updated {file_path} for version {version}.")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 6:
        print("Usage: python update_metainfo.py <file_path> <version> <date> <repo> <tag_name>")
        sys.exit(1)
    
    # Arguments: file_path, version, date, repo, tag_name
    updated = update_metainfo(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    if not updated:
        # We exit with 0 even if not updated (already exists) to not break the CI flow
        sys.exit(0)
