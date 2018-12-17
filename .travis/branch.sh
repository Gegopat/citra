if ! [[ $TAG_CREATED ]]; then
  export TAG_CREATED="1" &&
  git config --local user.name "Valentin Vanelslande"
  git config --local user.email "valentinvanelslandeacnl@gmail.com"
  git tag "$(git log --format=%h -1)"
fi
