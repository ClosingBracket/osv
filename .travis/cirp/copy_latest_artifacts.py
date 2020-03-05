# -*- coding: utf-8 -*-

from github import GithubObject
import logging

def copy_artifacts(src_dir, release_name):
    # 100 items per page is the max https://developer.github.com/v3/guides/traversing-with-pagination/#changing-the-number-of-items-received
    github = Github(login_or_token=github_token, base_url=github_api_url, per_page=100, timeout=config.timeout, retry=config.retries(), user_agent=config.user_agent)
    release = github.get_repo(github_repo_slug).get_release(release_name)
    logging.info('Uploading artifacts to "{}" release.'.format(release.tag_name))
    artifacts = sorted(os.listdir(src_dir))
    logging.info('Found {} artifact(s) in "{}" directory.'.format(len(artifacts), src_dir))
    for artifact in artifacts:
        artifact_path = os.path.join(src_dir, artifact)
        if os.path.isfile(artifact_path):
            logging.info('\tStoring "{}" ({} bytes) artifact in the release.'.format(artifact, os.path.getsize(artifact_path)))
            release.upload_asset(artifact_path)
    logging.info('All artifacts for "{}" release are uploaded.'.format(release.tag_name))
