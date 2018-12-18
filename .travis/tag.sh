git config --local user.name "Valentin Vanelslande"
git config --local user.email "valentinvanelslandeacnl@gmail.com"
git tag "$(git log --format=%h -1)"
