#!/usr/bin/python3

from github import Github
from github.GithubException import UnknownObjectException
import logging
import os, sys
import argparse

#pip install PyGithub

# Various GitHub helpers
def client(github_token):
    return Github(login_or_token=github_token, per_page=100)

def get_repo(args):
    gh = client(api_token)
    repo_full_name = "%s/%s" %(args.owner, args.repo)
    try:
        return gh.get_repo(repo_full_name)
    except UnknownObjectException:
        print("Repo %s does not exit!" % repo_full_name)
        return None

def list_releases(args):
    repository = get_repo(args)
    if repository:
        for release in repository.get_releases():
            print( "--> %s, %s" % (release.tag_name, release.title))

def get_release(args):
    repository = get_repo(args)
    if repository:
        try:
            return repository.get_release(args.release)
        except UnknownObjectException:
            print("Release %s does not exit!" % args.release)
            return None
    else:
        return None

def list_assets(args):
    release = get_release(args)
    if release:
        for asset in release.get_assets():
            print( "--> %s, %s" % (asset.name, asset.browser_download_url))

def upload_artifacts(args):
    def upload_artifact(release, artifact_path):
        if os.path.isfile(artifact_path):
            print('\tStoring "%s" (%d bytes) artifact in the release.' % (artifact_path, os.path.getsize(artifact_path)))
            release.upload_asset(artifact_path)

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
        print('All artifacts for "%s" release have been uploaded.' % release.tag_name)

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
    cmd_list_releases.set_defaults(func=list_releases)

    cmd_list_assets = subparsers.add_parser("list-assets", help="list release assets")
    cmd_list_assets.add_argument("--owner", action="store", required=True)
    cmd_list_assets.add_argument("--repo", action="store", required=True)
    cmd_list_assets.add_argument("--release", action="store", required=True)
    cmd_list_assets.set_defaults(func=list_assets)

    cmd_list_assets = subparsers.add_parser("upload-artifacts", help="upload release artifact/s")
    cmd_list_assets.add_argument("--owner", action="store", required=True)
    cmd_list_assets.add_argument("--repo", action="store", required=True)
    cmd_list_assets.add_argument("--release", action="store", required=True)
    cmd_list_assets.add_argument("--path", action="store", required=True)
    cmd_list_assets.set_defaults(func=upload_artifacts)

    args = parser.parse_args()
    args.func(args)