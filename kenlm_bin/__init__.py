import os
import pkg_resources
import subprocess

from typing import List


def call(executable: str, args_list: List[str], **popen_kwargs) -> int:
    executable_path = pkg_resources.resource_filename('kenlm_bin', f'bin/{executable}')
    if not os.path.exists(executable_path):
        raise Exception(f'Executable "{executable}" does not exist.')

    return subprocess.call([executable_path] + args_list, **popen_kwargs)
