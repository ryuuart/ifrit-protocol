"""
me - this DAT.

dat - the changed DAT
prevDAT - the DAT containing previous contents.

Info contains specific details on what's changed:
    rowsChanged	- list of row indices with different contents
    rowsAdded		- list of added row name indices (in dat)
    rowsRemoved	- list of removed row name indices (in prevDAT)

    colsChanged	- list of column indices with different contents
    colsAdded		- list of added column name indices (in dat)
    colsRemoved	- list of removed column name indices (in prevDAT)

    cellsChanged 	- list of cells that have changed content

    sizeChanged	- bool, true if number of rows or columns changed

Make sure the corresponding toggle is enabled in the DAT Execute DAT.

""" 
from SpellCircle.canvas import SCCanvas

def onTableChange(dat: DAT):
    """
    This callback can be used to evaluate several change conditions simultaneously.
    """
    return

def onRowChange(dat, rows):
	return

def onColChange(dat: DAT, cols):
    canvas = SCCanvas(width=10, height=10)

    for row in dat.rows()[1:]:
        x = float(row[1].val)
        y = float(row[2].val)
        # radius = float(row[4].val)
        radius = 1000

        canvas.circle("Potato", x * 10, y * 10, radius * 0.001 * 1, text_start=x * 2)

    op("udpout1").sendBytes(canvas.to_bytes())
    return

def onCellChange(dat, cells, prev):
    return

def onSizeChange(dat):
    return