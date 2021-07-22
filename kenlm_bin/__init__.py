import pkg_resources
import subprocess

from typing import List


def call(executable: str, args_list: List[str], **popen_kwargs) -> int:
    executable_path = pkg_resources.resource_filename('kenlm_bin', f'bin/{executable}')
    return subprocess.call([executable_path] + args_list, **popen_kwargs)
