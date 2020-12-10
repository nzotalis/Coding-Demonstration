from result_parser import *
import matplotlib.pyplot as plt

def main():
    output = ''
    years, percentages = [], []
    i = 1930
    while i <= 2018:
        if i != 1942 and i != 1946:
            output += '%d:\n' % i
            rep, perc = report(str(i))
            output += rep + '\n'
            years.append(i)
            percentages.append(perc)
        i += 4
    print(output)

    with open('report.txt', 'w') as r:
        r.write(output)
    '''
    plt.plot(years, percentages)
    plt.xlabel('Year')
    plt.ylabel('%non-European win')
    plt.grid(True)
    plt.xticks(years)
    plt.show()
    '''

if __name__ == "__main__":
    main()