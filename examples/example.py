import os
import kenlm

LM = os.path.join(os.path.dirname(__file__), '..', 'lm', 'test.arpa')
model = kenlm.LanguageModel(LM)
print('{0}-gram model'.format(model.order))

sentence = 'language modeling is fun .'
print(sentence)
print(model.score(sentence))

# Check that total full score = direct score
def score(s):
    return sum(prob for prob, _ in model.full_scores(s))

assert (abs(score(sentence) - model.score(sentence)) < 1e-3)

# Show scores and n-gram matches
words = ['<s>'] + sentence.split() + ['</s>']
for i, (prob, length) in enumerate(model.full_scores(sentence)):
    print('{0} {1} : {2}'.format(prob, length, ' '.join(words[i+2-length:i+2])))

# Find out-of-vocabulary words
for w in words:
    if not w in model:
        print('"{0}" is an OOV'.format(w))
