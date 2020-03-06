#!/usr/bin/python3

from github import Github
from github.GithubException import UnknownObjectException,GithubException
import logging
import os, sys
import argparse

#pip install PyGithub

OSV_REPO_NAME = 'cloudius-systems/osv'
OSV_NIGHTLY_RELEASES_REPO_NAME = 'osvunikernel/osv-nightly-releases'

# Various GitHub helpers
def client(github_token):
    return Github(login_or_token=github_token, per_page=100)

def get_repo(args):
    gh = client(api_token)
    repo_full_name = "%s/%s" %(args.owner, args.repo)
    try:
        return gh.get_repo(repo_full_name)
    except UnknownObjectException:
        print("Could not find the repo [%s]!" % repo_full_name)
        return None

def get_osv_repo():
    gh = client(api_token)
    return gh.get_repo(OSV_REPO_NAME)

def get_commit(repo, sha):
    try:
        commit = repo.get_commit(sha)
        return commit.commit
    except UnknownObjectException:
        print("Could not find the commit [%s]!" % sha)
        return None

def list_releases(args):
    def get_osv_commit_message(osv_repo, release_title):
        message = ''
        import re
        commit_sha = re.search('-g([a-f0-9]*)', release.title)  
        if commit_sha:
            commit = get_commit(osv_repo, commit_sha.group(1))
            if commit:
                message = commit.message.split('\n')[0]
        return message

    repository = get_repo(args)
    if repository:
        if args.message:
            osv_repo = get_osv_repo()
            print('%-18s %-30s %-20s %-30s' % (
                 "NAME", "TITLE", "PUBLISHED_AT", "MESSAGE"
            ))
        else:
            print('%-18s %-60s %-20s' % (
                 "NAME", "TITLE", "PUBLISHED_AT"
            ))
        all_releases = repository.get_releases()
        latest_releases = (r for r in all_releases if r.tag_name.endswith('latest'))
        non_latest_releases = (r for r in all_releases if not r.tag_name.endswith('latest'))
        from itertools import chain
        for release in chain(latest_releases, non_latest_releases):
            if args.message:
                message = get_osv_commit_message(osv_repo, release.title)
                print('%-18s %-30.30s %-20s %-30.30s' % (release.tag_name, release.title, str(release.published_at), message))
            else:
                print('%-18s %-60.60s %-20s' % (
                       release.tag_name, release.title, str(release.published_at)))

def get_release(args):
    repository = get_repo(args)
    if repository:
        try:
            return repository.get_release(args.release)
        except UnknownObjectException:
            print("Could not find the release [%s]!" % args.release)
            return None
    else:
        return None

def list_artifacts(args):
    release = get_release(args)
    if release:
        assets = release.get_assets()
        if assets.totalCount > 0:
            download_base = assets[0].browser_download_url.split('/download')[0]
            print('%-40s %-10s %-50s' % (
                  "NAME", "SIZE", "DOWNLOAD_URL (base=" + download_base  + "/download)"
            ))
            for asset in assets:
                download_url_suffix = asset.browser_download_url.split('/download')[1]
                print('%-40s %-10s %-50s' % (
                       asset.name, asset.size, download_url_suffix))
        else:
            print('Could not find any artifacts!')

def upload_artifacts(args):
    def upload_artifact(release, artifact_path):
        if os.path.isfile(artifact_path):
            try:
                release.upload_asset(artifact_path)
                print('Uploaded "%s" (%d bytes) artifact in the release.' % (artifact_path, os.path.getsize(artifact_path)))
            except GithubException:
                print('Failed to upload the artifact "%s"' % artifact_path)

    release = get_release(args)
    if release:
        print('Uploading artifacts to "%s" release.' % release.tag_name)
        if os.path.isdir(args.path):
            dir_path = args.path
            artifacts = sorted(os.listdir(dir_path))
            print('Found %d artifact(s) in "%s" directory.' % (len(artifacts), dir_path))
            for artifact in artifacts:
                artifact_path = os.path.join(dir_path, artifact)
                upload_artifact(release, artifact_path)
        else:
            upload_artifact(release, args.path)


def delete_artifacts(args):
    release = get_release(args)
    if release:
        found_assets = 0
        for asset in release.get_assets():
            import re
            if re.match(args.name, asset.name):
                found_assets = found_assets + 1
                answer = input("Would you like to delete artifact [%s] from the release: %s of %s? [y|n]" %
                                 (asset.name, args.release, "%s/%s" %(args.owner, args.repo)))
                if answer.capitalize() == 'Y':
                    asset.delete_asset()
                    print("Deleted artifact [%s] from the release: %s of %s!" %
                          (asset.name, args.release, "%s/%s" %(args.owner, args.repo)))
        if found_assets == 0:
            print('Failed to find any artifacts matching [%s]' % args.name)

def download_artifacts(args):
    release = get_release(args)
    if release:
        found_assets = 0
        for asset in release.get_assets():
            import re
            if re.match(args.name, asset.name):
                found_assets = found_assets + 1
                import subprocess
                ret = subprocess.call(['wget', '-nv', asset.browser_download_url, '-O', asset.name])
                if ret != 0:
                    print('Failed to download %s!' % download_url)
        if found_assets == 0:
            print('Failed to find any artifacts matching [%s]' % args.name)

api_token = os.environ.get('GH_API_TOKEN')

if __name__ == "__main__":
    if not api_token:
        print('Missing Github API token! Set GH_API_TOKEN')
        sys.exit(1)

    parser = argparse.ArgumentParser(description="github util")
    subparsers = parser.add_subparsers(help="Command")

    cmd_list_releases = subparsers.add_parser("list-releases", help="list releases")
    cmd_list_releases.add_argument("--owner", action="store", required=True)
    cmd_list_releases.add_argument("--repo", action="store", required=True)
    cmd_list_releases.add_argument("--message", action="store_true")
    cmd_list_releases.set_defaults(func=list_releases)

    cmd_list_artifacts = subparsers.add_parser("list-artifacts", help="list release artifacts")
    cmd_list_artifacts.add_argument("--owner", action="store", required=True)
    cmd_list_artifacts.add_argument("--repo", action="store", required=True)
    cmd_list_artifacts.add_argument("--release", action="store", required=True)
    cmd_list_artifacts.set_defaults(func=list_artifacts)

    cmd_upload_artifacts = subparsers.add_parser("upload-artifacts", help="upload release artifact/s")
    cmd_upload_artifacts.add_argument("--owner", action="store", required=True)
    cmd_upload_artifacts.add_argument("--repo", action="store", required=True)
    cmd_upload_artifacts.add_argument("--release", action="store", required=True)
    cmd_upload_artifacts.add_argument("--path", action="store", required=True)
    cmd_upload_artifacts.set_defaults(func=upload_artifacts)

    cmd_delete_artifacts = subparsers.add_parser("delete-artifact", help="delete release artifact")
    cmd_delete_artifacts.add_argument("--owner", action="store", required=True)
    cmd_delete_artifacts.add_argument("--repo", action="store", required=True)
    cmd_delete_artifacts.add_argument("--release", action="store", required=True)
    cmd_delete_artifacts.add_argument("--name", action="store", required=True)
    cmd_delete_artifacts.set_defaults(func=delete_artifacts)

    cmd_delete_artifacts = subparsers.add_parser("download-artifacts", help="delete release artifact")
    cmd_delete_artifacts.add_argument("--owner", action="store", required=True)
    cmd_delete_artifacts.add_argument("--repo", action="store", required=True)
    cmd_delete_artifacts.add_argument("--release", action="store", required=True)
    cmd_delete_artifacts.add_argument("--name", action="store", required=True)
    cmd_delete_artifacts.set_defaults(func=download_artifacts)

    args = parser.parse_args()
    args.func(args)
