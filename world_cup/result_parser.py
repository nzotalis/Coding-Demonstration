with open('europe.txt', 'r') as file:
    europe = [x.strip() for x in file.readlines()]

class Result:
    def __init__(self, team1, team2, score):
        self.team1 = team1
        self.team2 = team2
        self.score = score

    def __str__(self):
        return '%s %s %s' % (self.team1, self.score, self.team2)
    

def extract_scores(file, year):
    results = []
    if year == '1934':
        results = [Result('Italy', 'USA', '7-1'), Result('Spain', 'Brazil', '3-1'), Result('Hungary', 'Egypt', '4-2'), Result('Sweden', 'Argentina', '3-2')]
    lines = [x.strip() for x in file.readlines()]
    
    for i in range(len(lines)):
        if len(lines[i]) == 3 and lines[i][1] == '-':
            first_team = lines[i - 1]
            second_team = lines[i + 2]
            result = Result(first_team, second_team, lines[i])
            results.append(result)
    return results

def is_interesting(result):
    score = result.score
    if int(score[0]) > int(score[2]):
        winner = result.team1
        loser = result.team2
    elif int(score[0]) < int(score[2]):
        winner = result.team2
        loser = result.team1
    else:
        return False, False
    
    heterogeneous = (result.team1 in europe and result.team2 not in europe) or (result.team1 not in europe and result.team2 in europe)

    non_europe_winner = winner not in europe and loser in europe

    return heterogeneous, non_europe_winner
    
def report(year):
    output = ''
    if year == '1954':
        return '\nTotal: %d\nNon-European wins: %d\nEuropean Wins: %d\nPercent: %.3f\n'% (10, 3, 7, 3 / 10), 3/10
    with open("scores\\" + year + '.txt', 'r') as file:
        scores = extract_scores(file, year)
        i = 0
        n = 0
        if year == '1982':
            #Hungary 10-1 El Salvador
            n = 1
        for score in scores:
            interesting, win = is_interesting(score)
            if interesting:
                n += 1
                if win:
                    output += 'WIN: ' + str(score) + '\n'
                    i += 1
                else:
                    output += str(score) + '\n'
    return output + '\nTotal: %d\nNon-European wins: %d\nEuropean Wins: %d\nPercent: %.3f\n'% (n, i, n - i, i / n), i / n