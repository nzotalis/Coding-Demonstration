############################################################
# CMPSC442: Homework 4
############################################################

student_name = "Nicholas Zotalis"

############################################################
# Imports
############################################################

# Include your imports here, if any are used.
import copy

############################################################
# Section 1: Sudoku
############################################################
def sudoku_cells():
    return [(i, j) for i in range(9) for j in range(9)]

def sudoku_arcs():
    grids = get_grids()
    arcs = []
    for row1, col1 in sudoku_cells():
        for row2, col2 in sudoku_cells():
            if (row1 == row2 or col1 == col2 or \
            grids[(row1, col1)] == grids[(row2, col2)]) \
            and (row1, col1) != (row2, col2):
                arcs.append(((row1, col1), (row2, col2)))
    return set(arcs)
            
def read_board(path):
    with open(path) as f:
        board = f.readlines()
    dic = {}
    for i in range(9):
        for j in range(9):
            if board[i][j] == '*':
                dic[(i, j)] = set(list(range(1, 10)))
            else:
                dic[(i, j)] = set([int(board[i][j])])
    return dic

def get_grids():
    grids = {}
    for r in range(9):
        for c in range(9):
            row = r // 3
            col = c // 3
            grid = 3 * row + col
            grids[(r,c)] = grid
    return grids
    

class Sudoku(object):

    CELLS = sudoku_cells()
    ARCS = sudoku_arcs()

    def __init__(self, board):
        self.board = board

    def get_values(self, cell):
        return self.board[cell]

    def remove_inconsistent_values(self, cell1, cell2):
        removed = False
        if (cell1 != cell2) and (cell1, cell2) in self.ARCS:
            if len(self.board[cell2]) == 1 and self.board[cell2].issubset(self.board[cell1]):
                self.board[cell1] -= self.board[cell2]
                removed = True
        return removed

    def repeat_remove_inconsistent_values(self):
        removed = False
        for c1, c2 in self.ARCS:
            if self.remove_inconsistent_values(c1, c2):
                removed = True
        return removed

    def is_solved(self):
        for c1, c2 in self.ARCS:
            if len(self.board[c1]) != 1 or len(self.board[c2]) != 1 or self.board[c1] == self.board[c2]:
                return False
        return True

    def infer_ac3(self):
        while True:
            if not self.repeat_remove_inconsistent_values():
                break

    def check_set(self, cell, neighbors):
        for value in self.board[cell]:
            #assume this value is correct
            value_is_correct = True
            for n in neighbors:
                if value in self.board[n]:
                    value_is_correct = False
                    break
                
            #this value does not exist in any of its neighbors' sets
            if value_is_correct:
                return True, set([value])

        return False, None

    def remove_by_row_col_block(self):
        grids = get_grids()
        changed = False
        for c in self.CELLS:
            if len(self.board[c]) > 1:
                shared_row = []
                shared_col = []
                shared_grid = []
                #go again to grab all cells that share a column, row, or grid
                for c1 in self.CELLS:
                    if c != c1:
                        if c[0] == c1[0]:
                            shared_row.append(c1)
                        if c[1] == c1[1]:
                            shared_col.append(c1)
                        if grids[c] == grids[c1]:
                            shared_grid.append(c1)

                #see if there are any values for c that do not exist
                #in any of its shared cells, this will guarantee that
                #c should be this value.

                check_row, row_val = self.check_set(c, shared_row)
                if check_row:
                    self.board[c] = row_val
                    changed = True

                check_col, col_val = self.check_set(c, shared_col)
                if check_col:
                    self.board[c] = col_val
                    changed = True

                check_grid, grid_val = self.check_set(c, shared_grid)
                if check_grid:
                    self.board[c] = grid_val
                    changed = True
                    
        return changed
        
    def infer_improved(self):
        while True:
            firstboard = copy.deepcopy(self.board)
            self.infer_ac3()

            while True:
                if not self.remove_by_row_col_block():
                    break

            if self.is_solved():
                return True
            
            elif firstboard == self.board:
                return False

    def successors(self):
        for c in self.CELLS:
            if len(self.board[c]) > 1:
                for v in self.board[c]:
                    guess = copy.deepcopy(self.board)
                    guess[c] = set([v])
                    yield guess
                break
    
    def infer_with_guessing(self):
        boards = [copy.deepcopy(self.board)]

        while len(boards) > 0:
            guess = boards.pop()
            self.board = guess
            if self.infer_improved():
                break
            else:
                boards += list(self.successors())

############################################################
# Section 2: Feedback
############################################################

feedback_question_1 = """
About 8 hours
"""

feedback_question_2 = """
The improved inference was the hardest part. It was hard to figure out how to implement a function
to check if a cell's set contained any values not found in its neighbors' sets.
"""

feedback_question_3 = """
I liked that each successive function used the ones that came before as a subroutine. One thing
I would change is the wording on the remove_inconsistent_values description, as it is confusing.
"""

b = read_board("sudoku_boards/hardest.txt")
s = Sudoku(b)

from datetime import datetime
t1 = datetime.now()
s.infer_with_guessing()
t2 = datetime.now()


sorted_board = sorted(s.board)

for c in range(9):
    out = ""
    for i in range(9):
        out += str(list(s.board[(c, i)])[0]) + "  "
    print(out)
    
print((t2-t1).total_seconds())
