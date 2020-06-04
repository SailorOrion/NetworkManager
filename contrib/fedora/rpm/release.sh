#!/bin/bash

#
# You need to start with a clean working directory of NetworkManager
# and all branches up to date.
#

die() {
    echo "FAIL: $@"
    exit 1
}

die_usage() {
    echo "FAIL: $@"
    echo
    echo "Usage:"
    echo "  $0 [minor|devel|rc] [--no-test] [--no-find-backports] [--no-cleanup]"
    exit 1
}

parse_version() {
    local MAJ="$(sed -n '1,20 s/^m4_define(\[nm_major_version\], \[\([0-9]\+\)\])$/\1/p' configure.ac)"
    local MIN="$(sed -n '1,20 s/^m4_define(\[nm_minor_version\], \[\([0-9]\+\)\])$/\1/p' configure.ac)"
    local MIC="$(sed -n '1,20 s/^m4_define(\[nm_micro_version\], \[\([0-9]\+\)\])$/\1/p' configure.ac)"

    re='^[0-9]+ [0-9]+ [0-9]+$'
    [[ "$MAJ $MIN $MIC" =~ $re ]] || return 1
    echo "$MAJ $MIN $MIC"
}

number_is_even() {
    local re='^[0-9]*[02468]$'
    [[ "$1" =~ $re ]]
}

number_is_odd() {
    local re='^[0-9]*[13579]$'
    [[ "$1" =~ $re ]]
}

git_same_ref() {
    local a="$(git rev-parse "$1" 2>/dev/null)" || return 1
    local b="$(git rev-parse "$2" 2>/dev/null)" || return 1
    [ "$a" = "$b" ]
}

set_version_number_autotools() {
    sed -i \
        -e '1,20 s/^m4_define(\[nm_major_version\], \[\([0-9]\+\)\])$/m4_define([nm_major_version], ['"$1"'])/' \
        -e '1,20 s/^m4_define(\[nm_minor_version\], \[\([0-9]\+\)\])$/m4_define([nm_minor_version], ['"$2"'])/' \
        -e '1,20 s/^m4_define(\[nm_micro_version\], \[\([0-9]\+\)\])$/m4_define([nm_micro_version], ['"$3"'])/' \
        ./configure.ac
}

set_version_number_meson() {
    sed -i \
        -e '1,20 s/^\( *version: *'\''\)[0-9]\+\.[0-9]\+\.[0-9]\+\('\'',\)$/\1'"$1.$2.$3"'\2/' \
        meson.build
}

set_version_number() {
    set_version_number_autotools "$@" &&
    set_version_number_meson "$@"
}

DO_CLEANUP=1
CLEANUP_CHECKOUT_BRANCH=
CLEANUP_REFS=()
cleanup() {
    if [ $DO_CLEANUP = 1 ]; then
        for c in "${CLEANUP_REFS[@]}"; do
            git update-ref -d "$c"
            [ -n "$CLEANUP_CHECKOUT_BRANCH" ] && git checkout -f "$CLEANUP_CHECKOUT_BRANCH"
        done
    fi
}

trap cleanup EXIT

DIR="$(git rev-parse --show-toplevel)"

ORIGIN=origin

test -d "$DIR" &&
cd "$DIR" &&
test -f ./src/NetworkManagerUtils.h &&
test -f ./contrib/fedora/rpm/build_clean.sh || die "cannot find NetworkManager base directory"

TMP="$(git status --porcelain)" || die "git status failed"
test -z "$TMP" || die "git working directory is not clean (git status --porcelain)"

TMP="$(LANG=C git clean -ndx)" || die "git clean -ndx failed"
test -z "$TMP" || die "git working directory is not clean (git clean -ndx)"

VERSION_ARR=( $(parse_version) ) || die "cannot detect NetworkManager version"
VERSION_STR="$(IFS=.; echo "${VERSION_ARR[*]}")"

RELEASE_MODE=""
DRY_RUN=1
FIND_BACKPORTS=1
while [ "$#" -ge 1 ]; do
    A="$1"
    shift
    if [ -z "$RELEASE_MODE" ]; then
        case "$A" in
            minor|devel|rc)
                RELEASE_MODE="$A"
                ;;
            *)
                ;;
        esac
        continue
    fi
    case "$A" in
        --no-test)
            DRY_RUN=0
            ;;
        --no-find-backports)
            FIND_BACKPORTS=0
            ;;
        --no-cleanup)
            DO_CLEANUP=0
            ;;
        *)
            die_usage "unknown argument \"$A\""
            ;;
    esac
done
[ -n "$RELEASE_MODE" ] || die_usage "specify the desired release mode"

echo "Current version before release: $VERSION_STR (do $RELEASE_MODE release)"

CUR_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
TMP_BRANCH=release-branch

if [ "$CUR_BRANCH" = master ]; then
    number_is_odd "${VERSION_ARR[1]}" || die "Unexpected version number on master. Should be an odd development version"
else
    re='^nm-[0-9]+-[0-9]+$'
    [[ "$CUR_BRANCH" =~ $re ]] || die "Unexpected current branch $CUR_BRANCH. Should be master or nm-?-??"
    if number_is_odd "${VERSION_ARR[1]}"; then
        # we are on a release candiate branch.
        [ "$RELEASE_MODE" = rc ] || "Unexpected branch name \"$CUR_BRANCH\" for \"$RELEASE_MODE\""
        [ "$CUR_BRANCH" == "nm-${VERSION_ARR[0]}-$((${VERSION_ARR[1]} + 1))" ] || die "Unexpected current branch $CUR_BRANCH. Should be nm-${VERSION_ARR[0]}-$((${VERSION_ARR[1]} + 1))"
    else
        [ "$CUR_BRANCH" == "nm-${VERSION_ARR[0]}-${VERSION_ARR[1]}" ] || die "Unexpected current branch $CUR_BRANCH. Should be nm-${VERSION_ARR[0]}-${VERSION_ARR[1]}"
    fi
fi

case "$RELEASE_MODE" in
    minor)
        number_is_even "${VERSION_ARR[1]}" &&
        number_is_odd  "${VERSION_ARR[2]}" || die "cannot do minor release on top of version $VERSION_STR"
        [ "$CUR_BRANCH" != master ] || die "cannot do a minor release on master"
        ;;
    devel)
        number_is_odd "${VERSION_ARR[1]}" || die "cannot do devel release on top of version $VERSION_STR"
        [ "$((${VERSION_ARR[2]} + 1))" -lt 90 ] || die "devel release must have a micro version smaller than 90 but current version is $VERSION_STR"
        [ "$CUR_BRANCH" == master ] || die "devel release can only be on master"
        ;;
    *)
        die "Release mode $RELEASE_MODE not yet implemented"
        ;;
esac

git fetch || die "git fetch failed"

git_same_ref "$CUR_BRANCH" "refs/heads/$CUR_BRANCH" || die "Current branch $CUR_BRANCH is not a branch??"
git_same_ref "$CUR_BRANCH" "refs/remotes/$ORIGIN/$CUR_BRANCH" || die "Current branch $CUR_BRANCH seems not up to date. Git pull?"

NEWER_BRANCHES=()
if [ "$CUR_BRANCH" != master ]; then
    i="${VERSION_ARR[1]}"
    while : ; do
        i=$((i + 2))
        b="nm-${VERSION_ARR[0]}-$i"
        if ! git show-ref --verify --quiet "refs/remotes/$ORIGIN/$b"; then
            git show-ref --verify --quiet "refs/heads/$b" && die "unexpectedly branch $b exists"
            break
        fi
        git_same_ref "$b" "refs/heads/$b" || die "branch $b is not a branch??"
        git_same_ref "$b" "refs/remotes/$ORIGIN/$b" || die "branch $b seems not up to date. Git pull?"
        NEWER_BRANCHES+=("refs/heads/$b")
    done
    b=master
    git_same_ref "$b" "refs/heads/$b" || die "branch $b is not a branch??"
    git_same_ref "$b" "refs/remotes/$ORIGIN/$b" || die "branch $b seems not up to date. Git pull?"
fi

if [ $FIND_BACKPORTS = 1 ]; then
    git show "$ORIGIN/automation:contrib/rh-utils/find-backports.sh" > ./.git/nm-find-backports.sh \
    && chmod +x ./.git/nm-find-backports.sh \
    || die "cannot get contrib/rh-utils/find-backports.sh"

    TMP="$(./.git/nm-find-backports.sh "$(git merge-base master HEAD)" "$CUR_BRANCH" master "${NEWER_BRANCHES[@]}")" || die "nm-find-backports failed"
    test -z "$TMP" || die "nm-find-backports returned patches that need to be backported: ./.git/nm-find-backports.sh \"\$(git merge-base master HEAD)\" \"$CUR_BRANCH\" master ${NEWER_BRANCHES[@]}"
fi

TAGS=()
BUILD_TAG=

CLEANUP_CHECKOUT_BRANCH="$CUR_BRANCH"

case "$RELEASE_MODE" in
    minor)
        git checkout -B "$TMP_BRANCH"
        CLEANUP_REFS+=("refs/heads/$TMP_BRANCH")
        set_version_number "${VERSION_ARR[0]}" "${VERSION_ARR[1]}" $(("${VERSION_ARR[2]}" + 1))
        git commit -m "release: bump version to ${VERSION_ARR[0]}.${VERSION_ARR[1]}.$(("${VERSION_ARR[2]}" + 1))" -a || die "failed to commit release"
        set_version_number "${VERSION_ARR[0]}" "${VERSION_ARR[1]}" $(("${VERSION_ARR[2]}" + 2))
        git commit -m "release: bump version to ${VERSION_ARR[0]}.${VERSION_ARR[1]}.$(("${VERSION_ARR[2]}" + 2)) (development)" -a || die "failed to commit devel version bump"

        b="${VERSION_ARR[0]}.${VERSION_ARR[1]}.$(("${VERSION_ARR[2]}" + 1))"
        git tag -s -a -m "Tag $b" "$b" HEAD~ || die "failed to tag release"
        TAGS+=("$b")
        CLEANUP_REFS+=("refs/tags/$b")
        BUILD_TAG="$b"
        b="${VERSION_ARR[0]}.${VERSION_ARR[1]}.$(("${VERSION_ARR[2]}" + 2))"
        git tag -s -a -m "Tag $b (development)" "$b-dev" HEAD || die "failed to tag devel version"
        TAGS+=("$b-dev")
        CLEANUP_REFS+=("refs/tags/$b-dev")
        ;;
    devel)
        git checkout -B "$TMP_BRANCH"
        CLEANUP_REFS+=("refs/heads/$TMP_BRANCH")
        set_version_number "${VERSION_ARR[0]}" "${VERSION_ARR[1]}" $(("${VERSION_ARR[2]}" + 1))
        git commit -m "release: bump version to ${VERSION_ARR[0]}.${VERSION_ARR[1]}.$(("${VERSION_ARR[2]}" + 1)) (development)" -a || die "failed to commit devel version bump"

        b="${VERSION_ARR[0]}.${VERSION_ARR[1]}.$(("${VERSION_ARR[2]}" + 1))"
        git tag -s -a -m "Tag $b (development)" "$b-dev" HEAD || die "failed to tag release"
        TAGS+=("$b-dev")
        CLEANUP_REFS+=("refs/tags/$b-dev")
        BUILD_TAG="$b-dev"
        ;;
    *)
        die "Release mode $RELEASE_MODE not yet implemented"
        ;;
esac

RELEASE_FILE=

if [ -n "$BUILD_TAG" ]; then
    git checkout "$BUILD_TAG" || die "failed to checkout $BUILD_TAG"

    ./contrib/fedora/rpm/build_clean.sh -r || die "build release failed"

    RELEASE_FILE="NetworkManager-${BUILD_TAG%-dev}.tar.xz"

    test -f "./$RELEASE_FILE" || die "release file \"./$RELEASE_FILE\" not found"
fi

if [ -n "$RELEASE_FILE" ]; then
    if [ $DRY_RUN = 1 ]; then
        echo "COMMAND: rsync -va --append-verify -P \"./$RELEASE_FILE\" master.gnome.org:"
        echo "COMMAND: ssh master.gnome.org ftpadmin install \"$RELEASE_FILE\""
    else
        rsync -va --append-verify -P "./$RELEASE_FILE" master.gnome.org: &&
        rsync -va --append-verify -P "./$RELEASE_FILE" master.gnome.org: || die "failed to rsync \"$RELEASE_FILE\""
        ssh master.gnome.org ftpadmin install "$RELEASE_FILE"
    fi
fi

git checkout "$CUR_BRANCH" || die "cannot checkout $CUR_BRANCH"
git merge --ff-only "$TMP_BRANCH" || die "cannot git merge --ff-only $TMP_BRANCH"

if [ $DRY_RUN = 1 ]; then
    echo "COMMAND: git push \"$ORIGIN\" \"${TAGS[@]}\" \"$CUR_BRANCH\""
else
    git push "$ORIGIN" "${TAGS[@]}" "$CUR_BRANCH"
fi

if [ $DRY_RUN != 1 ]; then
    CLEANUP_REFS=()
    CLEANUP_CHECKOUT_BRANCH=
fi