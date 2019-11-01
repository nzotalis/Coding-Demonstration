############################################################
# CMPSC442: Homework 5
############################################################

student_name = "Nicholas Zotalis"

############################################################
# Imports
############################################################

# Include your imports here, if any are used.
import email.iterators
import math
import os

############################################################
# Section 1: Spam Filter
############################################################

def load_tokens(email_path):
    toks = []
    with open(email_path) as file:
        m = email.message_from_file(file)

    for bli in email.iterators.body_line_iterator(m):
        toks += bli.split()

    return toks

def log_probs(email_paths, smoothing):
    counts = {}
    totalWords = 0
    for ep in email_paths:
        for t in load_tokens(ep):
            totalWords += 1
            try:
                counts[t] += 1
            except KeyError:
                counts[t] = 1
                
    smoothed = {}
    for word in counts:
        smoothed[word] = math.log((counts[word] + smoothing) / (totalWords + smoothing * (totalWords + 1)))

    smoothed['<UNK>'] = math.log(smoothing / (totalWords + smoothing * (totalWords + 1)))
    return smoothed

class SpamFilter(object):

    def __init__(self, spam_dir, ham_dir, smoothing):
        spam_files = ['homework5_data/train/spam/' + x for x in os.listdir(spam_dir)]
        ham_files = ['homework5_data/train/ham/' + x for x in os.listdir(ham_dir)]

        self.spam_log_probs = log_probs(spam_files, smoothing)
        self.ham_log_probs = log_probs(ham_files, smoothing)

        self.PSPAM = float(len(spam_files)) / float((len(spam_files) + len(ham_files)))
        self.PHAM = 1 - self.PSPAM
    
    def is_spam(self, email_path):
        counts = {}
        for t in load_tokens(email_path):
            try:
                counts[t] += 1
            except KeyError:
                counts[t] = 1

        p_spam = 0
        p_ham = 0

        
        for t in counts:
            if t in self.spam_log_probs:
                p_spam += (self.spam_log_probs[t] * counts[t])
            else:
                p_spam += (self.spam_log_probs['<UNK>'] * counts[t])

            if t in self.ham_log_probs:
                p_ham += (self.ham_log_probs[t] * counts[t])
            else:
                p_ham += (self.ham_log_probs['<UNK>'] * counts[t])
                
        p_spam *= self.PSPAM
        p_ham *= self.PHAM

        return p_spam > p_ham
        
    def most_indicative_spam(self, n):
        spam_indic = {}
        words = set(self.spam_log_probs.keys()) & set(self.ham_log_probs.keys())

        for word in words:
            spam_indic[word] = self.spam_log_probs[word] - math.log(math.exp(self.spam_log_probs[word]) + math.exp(self.ham_log_probs[word]))
        
        spam_indic = sorted(spam_indic, key=spam_indic.get, reverse = True)
        return spam_indic[:n]

    
    def most_indicative_ham(self, n):
        ham_indic = {}
        words = set(self.spam_log_probs.keys()) & set(self.ham_log_probs.keys())

        for word in words:
            ham_indic[word] = self.ham_log_probs[word] - math.log(math.exp(self.ham_log_probs[word]) + math.exp(self.spam_log_probs[word]))

        ham_indic = sorted(ham_indic, key=ham_indic.get, reverse = True)
        return ham_indic[:n]

############################################################
# Section 2: Feedback
############################################################

feedback_question_1 = """
10 hours
"""

feedback_question_2 = """
The hardest part was realizing how to change the formula in part 4 so that I could use the log probs from earlier
"""

feedback_question_3 = """
I liked the most indicative spam/ham part.
"""
