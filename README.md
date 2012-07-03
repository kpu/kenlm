# python-kenlm

A python interface to [kenlm](http://kheafield.com/code/kenlm/)

## Installation

```bash
pip install -e git+https://github.com/vchahun/kenlm.git#egg=kenlm
```

## Basic Usage
```python
import kenlm
model = kenlm.LanguageModel('examples/mini.klm')
sentence = u'this is a sentence .'
print model.score(sentence)
```
