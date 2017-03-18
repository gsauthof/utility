
# i.e. the checked out git working directory
# currently travis uses $HOME/build/$TRAVIS_REPO_SLUG
# as CWD of a build job
src=$PWD
build_root=$HOME/art
build=$build_root/$TRAVIS_REPO_SLUG
