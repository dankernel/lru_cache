
git rm --cached giant_file
git rm data/proj_4.csv
git commit --amend -CHEAD
git push
git push
git filter-branch -f --index-filter 'git rm --cached --ignore-unmatch BADFILENAME'
git filter-branch -f --index-filter 'git rm --cached --ignore-unmatch data/proj_4.csv'
