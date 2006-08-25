// BookmarkDlg.cpp : implements dialog for viewing/editing bookmarks
//
// Copyright (c) 2003 by Andrew W. Phillips.
//
// No restrictions are placed on the noncommercial use of this code,
// as long as this text (from the above copyright notice to the
// disclaimer below) is preserved.
//
// This code may be redistributed as long as it remains unmodified
// and is not sold for profit without the author's written consent.
//
// This code, or any part of it, may not be used in any software that
// is sold for profit, without the author's written consent.
//
// DISCLAIMER: This file is provided "as is" with no expressed or
// implied warranty. The author accepts no liability for any damage
// or loss of business that this product may cause.
//

#include "stdafx.h"

#include <io.h>

#include "HexEdit.h"
#include "MainFrm.h"
#include "HexEditDoc.h"
#include "HexEditView.h"
#include "Bookmark.h"
#include "BookmarkDlg.h"

#ifdef USE_HTML_HELP
#include <HtmlHelp.h>
#endif

#include "resource.hm"
#include "HelpID.hm"            // For dlg help ID

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// We need access to the grid since the callback sorting function does not have
// access to it and we need to know which column we are sorting on (GetSortColumn).
static CGridCtrl *p_grid;

// The sorting callback function can't be a member function
static int CALLBACK bl_compare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    int retval;

	(void)lParamSort;

	CGridCellBase* pCell1 = (CGridCellBase*) lParam1;
	CGridCellBase* pCell2 = (CGridCellBase*) lParam2;
	if (!pCell1 || !pCell2) return 0;

    ASSERT(p_grid->GetSortColumn() < p_grid->GetColumnCount());
	switch (p_grid->GetSortColumn() - p_grid->GetFixedColumnCount())
	{
    case CBookmarkDlg::COL_NAME:
    case CBookmarkDlg::COL_LOCN:
		// Do text compare (ignore case)
#if _MSC_VER >= 1300
		return _wcsicmp(pCell1->GetText(), pCell2->GetText());
#else
		return _stricmp(pCell1->GetText(), pCell2->GetText());
#endif
    case CBookmarkDlg::COL_FILE:
#if _MSC_VER >= 1300
        retval = _wcsicmp(pCell1->GetText(), pCell2->GetText());
        if (retval != 0)
            return retval;
        // Now compare based on file directory
        pCell1 = (CGridCellBase*)pCell1->GetData();
        pCell2 = (CGridCellBase*)pCell2->GetData();
        retval = _wcsicmp(pCell1->GetText(), pCell2->GetText());
#else
        retval = _stricmp(pCell1->GetText(), pCell2->GetText());
        if (retval != 0)
            return retval;
        // Now compare based on file directory
        pCell1 = (CGridCellBase*)pCell1->GetData();
        pCell2 = (CGridCellBase*)pCell2->GetData();
        retval = _stricmp(pCell1->GetText(), pCell2->GetText());
#endif
        if (retval != 0)
            return retval;
        // Now compare based on file position
        pCell1 = (CGridCellBase*)pCell1->GetData();
        pCell2 = (CGridCellBase*)pCell2->GetData();
		if (pCell1->GetData() < pCell2->GetData())
			return -1;
		else if (pCell1->GetData() == pCell2->GetData())
			return 0;
		else
			return 1;
        break;
    case CBookmarkDlg::COL_POS:
    case CBookmarkDlg::COL_MODIFIED:
    case CBookmarkDlg::COL_ACCESSED:
		// Do date compare
		if (pCell1->GetData() < pCell2->GetData())
			return -1;
		else if (pCell1->GetData() == pCell2->GetData())
			return 0;
		else
			return 1;
        break;
	}
	ASSERT(0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CBookmarkDlg dialog

CBookmarkDlg::CBookmarkDlg(CWnd* pParent /*=NULL*/)
{
	p_grid = &grid_;
    pdoc_ = NULL;

	m_sizeInitial = CSize(-1, -1);
}

BOOL CBookmarkDlg::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
    return Create(pParentWnd);
}

BOOL CBookmarkDlg::Create(CWnd* pParentWnd) 
{
	if (!CHexDialogBar::Create(CBookmarkDlg::IDD, pParentWnd, CBRS_LEFT | CBRS_SIZE_DYNAMIC))
        return FALSE;
    // We need to setup the resizer here to make sure it gets a WM_SIZE message for docked window
	resizer_.Create(GetSafeHwnd(), TRUE, 100, TRUE); // 4th param when dlg = child of resized window

	// Default to retaining network/removaeable drive files when validating
	ASSERT(GetDlgItem(IDC_NET_RETAIN) != NULL);
	((CButton *)GetDlgItem(IDC_NET_RETAIN))->SetCheck(BST_CHECKED);

	ASSERT(GetDlgItem(IDC_GRID_BL) != NULL);
    if (!grid_.SubclassWindow(GetDlgItem(IDC_GRID_BL)->m_hWnd))
    {
        TRACE0("Failed to subclass grid control\n");
		return FALSE;
    }

    // Base min size on dialog resource height + half width
	CSize tmp_size;
	if (m_sizeInitial.cx == -1)
	{
		// Get window size (not client size) from BCGControlBar and adjust for edges + caption
		tmp_size = m_sizeDefault;
		tmp_size.cx -= 2*GetSystemMetrics(SM_CXFIXEDFRAME);
		tmp_size.cy -= 2*GetSystemMetrics(SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CYCAPTION);
	}
	else
	{
		tmp_size = m_sizeInitial;
	}
	resizer_.SetInitialSize(tmp_size);
	tmp_size.cx /= 2;
	resizer_.SetMinimumTrackingSize(tmp_size);

    resizer_.Add(IDC_BOOKMARK_NAME, 0, 0, 100, 0);
    resizer_.Add(IDC_BOOKMARK_ADD, 100, 0, 0, 0);
    resizer_.Add(IDC_GRID_BL, 0, 0, 100, 100);
    resizer_.Add(IDOK, 100, 0, 0, 0);
    resizer_.Add(IDC_BOOKMARK_GOTO, 100, 0, 0, 0);
    resizer_.Add(IDC_BOOKMARK_REMOVE, 100, 0, 0, 0);
    resizer_.Add(IDC_BOOKMARKS_VALIDATE, 100, 0, 0, 0);
    resizer_.Add(IDC_NET_RETAIN, 100, 0, 0, 0);
    resizer_.Add(IDC_BOOKMARKS_HELP, 100, 0, 0, 0);

    // Set up the grid control
    grid_.SetDoubleBuffering();
    grid_.SetAutoFit();
	grid_.SetCompareFunction(&bl_compare);
    grid_.SetGridLines(GVL_BOTH); // GVL_HORZ | GVL_VERT
    grid_.SetTrackFocusCell(FALSE);
    grid_.SetFrameFocusCell(FALSE);
    grid_.SetListMode(TRUE);
    grid_.SetSingleRowSelection(FALSE);
    grid_.SetHeaderSort(TRUE);

    grid_.SetFixedRowCount(1);

    InitColumnHeadings();
    grid_.SetColumnResize();

    grid_.EnableRowHide(FALSE);
    grid_.EnableColumnHide(FALSE);
    grid_.EnableHiddenRowUnhide(FALSE);
    grid_.EnableHiddenColUnhide(FALSE);

    FillGrid();

    grid_.ExpandColsNice(FALSE);

    // We need this so that the resizer gets WM_SIZE event after the controls
    // have been added.
    CRect cli;
    GetClientRect(&cli);
    PostMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(cli.Width(), cli.Height()));
    return TRUE;
}

BEGIN_MESSAGE_MAP(CBookmarkDlg, CHexDialogBar)
	//{{AFX_MSG_MAP(CBookmarkDlg)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BOOKMARK_ADD, OnAdd)
	ON_BN_CLICKED(IDC_BOOKMARK_GOTO, OnGoTo)
	ON_BN_CLICKED(IDC_BOOKMARK_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_BOOKMARKS_VALIDATE, OnValidate)
	ON_WM_SIZE()
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_BOOKMARKS_HELP, OnHelp)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, OnOK)
    ON_WM_CONTEXTMENU()
    ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
    ON_NOTIFY(NM_CLICK, IDC_GRID_BL, OnGridClick)
    ON_NOTIFY(NM_DBLCLK, IDC_GRID_BL, OnGridDoubleClick)
    ON_NOTIFY(NM_RCLICK, IDC_GRID_BL, OnGridRClick)
	//ON_MESSAGE_VOID(WM_INITIALUPDATE, OnInitialUpdate)
END_MESSAGE_MAP()

void CBookmarkDlg::InitColumnHeadings()
{
    static char *heading[] =
    {
        "Name",
        "File",
        "Folder",
        "Byte No",
        "Modified",
        "Accessed",
        NULL
    };

    ASSERT(sizeof(heading)/sizeof(*heading) == COL_LAST + 1);

    CString strWidths = theApp.GetProfileString("File-Settings", 
		                                        "BookmarkDialogColumns",
												"80,80,,60,,138");
    int curr_col = grid_.GetFixedColumnCount();
    bool all_hidden = true;

    for (int ii = 0; ii < COL_LAST; ++ii)
    {
        CString ss;

        AfxExtractSubString(ss, strWidths, ii, ',');
        int width = atoi(ss);
        if (width != 0) all_hidden = false;                  // we found a visible column

        ASSERT(heading[ii] != NULL);
        grid_.SetColumnCount(curr_col + 1);
        if (width == 0)
            grid_.SetColumnWidth(curr_col, 0);              // make column hidden
        else
            grid_.SetUserColumnWidth(curr_col, width);      // set user specified size (or -1 to indicate fit to cells)

        // Set column heading text (centred).  Also set item data so we know what goes in this column
        GV_ITEM item;
        item.row = 0;                                       // top row is header
        item.col = curr_col;                                // column we are changing
        item.mask = GVIF_PARAM|GVIF_FORMAT|GVIF_TEXT;       // change data+centered+text
        item.lParam = ii;                                   // data that says what's in this column
        item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE;  // centre the heading
        item.strText = heading[ii];                         // text of the heading
        grid_.SetItem(&item);

        ++curr_col;
    }

    // Show at least one column
    if (all_hidden)
    {
        grid_.SetColumnWidth(grid_.GetFixedColumnCount(), 1);
        grid_.AutoSizeColumn(grid_.GetFixedColumnCount(), GVS_BOTH);
    }
}

void CBookmarkDlg::FillGrid()
{
    CHexEditView *pview;                // Active view
    CHexEditDoc *pdoc = NULL;           // Active document (doc of the active view)
    int first_sel_row = -1;             // Row of first bookmark found in the active document

    if (FALSE && (pview = GetView()) != NULL)
        pdoc = pview->GetDocument();

    // Load all the bookmarks into the grid control
    CBookmarkList *pbl = theApp.GetBookmarkList();
    ASSERT(pbl->name_.size() == pbl->file_.size());

    int index, row;
    for (index = pbl->name_.size()-1, row = grid_.GetFixedRowCount(); index >= 0; index--)
	{
        // Don't show empty (deleted) names or those starting with an underscore (internal use)
		if (!pbl->name_[index].IsEmpty() && pbl->name_[index][0] != '_')
		{
		    grid_.SetRowCount(row + 1);
            // If this bookmark is in the currently active document then select it
            if (pdoc != NULL && std::find(pdoc->bm_index_.begin(), pdoc->bm_index_.end(), index) != pdoc->bm_index_.end())
            {
                if (first_sel_row == -1) first_sel_row = row;
                UpdateRow(index, row, TRUE);
            }
            else
                UpdateRow(index, row);
			++row;
		}
	}
    if (first_sel_row != -1)
        grid_.EnsureVisible(first_sel_row, 0);
}

void CBookmarkDlg::RemoveBookmark(int index)
{
    // First find the bookmark in the list
    int row;
	int col = COL_NAME + grid_.GetFixedColumnCount();
	for (row = grid_.GetFixedRowCount(); row < grid_.GetRowCount(); ++row)
	{
		if (grid_.GetItemData(row, col) == index)
			break;
	}
	if (row < grid_.GetRowCount())
    {
        grid_.DeleteRow(row);
        grid_.Refresh();
    }
}

// Given a bookmark it updates the row in the dialog.  First it finds the
// row or appends a row if the bookmark is not found then calls UpdateRow
void CBookmarkDlg::UpdateBookmark(int index, BOOL select /*=FALSE*/)
{
    // First find the bookmark in the list
    int row;
	int col = COL_NAME + grid_.GetFixedColumnCount();
	for (row = grid_.GetFixedRowCount(); row < grid_.GetRowCount(); ++row)
	{
		if (grid_.GetItemData(row, col) == index)
			break;
	}
	if (row == grid_.GetRowCount())
    {
        // Not found so append a row and update that
		grid_.SetRowCount(row + 1);
    }

    UpdateRow(index, row, select);
}

// Given a bookmark (index) and a row in the grid control (row) update
// the row with the bookmark info and (optionally) select the row
void CBookmarkDlg::UpdateRow(int index, int row, BOOL select /*=FALSE*/)
{
    CBookmarkList *pbl = theApp.GetBookmarkList();

    int fcc = grid_.GetFixedColumnCount();

#if 0
    // If the file is already open we have to check the document to get the latest position
    if (last_file_ != pbl->file_[index])
    {
        if (theApp.m_pDocTemplate->MatchDocType(pbl->file_[index], pdoc_) != 
            CDocTemplate::yesAlreadyOpen)
        {
            pdoc_ = NULL;
        }
        last_file_ = pbl->file_[index];
    }
#endif

    GV_ITEM item;
	item.row = row;
    item.mask = GVIF_STATE|GVIF_FORMAT|GVIF_TEXT|GVIF_PARAM;
    item.nState = GVIS_READONLY;
    if (select) item.nState |= GVIS_SELECTED;

    struct tm *timeptr;             // Used in displaying dates
    char disp[128];

    // Split filename into name and path
    int path_len;                   // Length of path (full name without filename)
    path_len = pbl->file_[index].ReverseFind('\\');
    if (path_len == -1) path_len = pbl->file_[index].ReverseFind('/');
    if (path_len == -1) path_len = pbl->file_[index].ReverseFind(':');
    if (path_len == -1)
        path_len = 0;
    else
        ++path_len;

    int ext_pos = pbl->file_[index].ReverseFind('.');
    if (ext_pos < path_len) ext_pos = -1;              // '.' in a sub-directory name

    for (int column = 0; column < COL_LAST; ++column)
    {
        item.col = column + fcc;

        switch (column)
        {
        case COL_NAME:
			item.nFormat = DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            item.strText = pbl->name_[index];
			item.lParam = index;
            break;
        case COL_FILE:
			item.nFormat = DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            item.strText = pbl->file_[index].Mid(path_len);
			item.lParam = (LONGLONG)grid_.GetCell(item.row, fcc + COL_LOCN);
            break;
        case COL_LOCN:
			item.nFormat = DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            item.strText = pbl->file_[index].Left(path_len);
			item.lParam = (LONGLONG)grid_.GetCell(item.row, fcc + COL_POS);
            break;
        case COL_POS:
			item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            FILE_ADDRESS addr;
            if (pdoc_ != NULL)
                addr = __int64(((CHexEditDoc *)pdoc_)->GetBookmarkPos(index));
            else
                addr = __int64(pbl->filepos_[index]);
            sprintf(disp, "%I64d", addr);
            item.strText = disp;
            ::AddCommas(item.strText);
    		item.lParam = addr;          // lParam is now 64 bit
            break;
        case COL_MODIFIED:
			item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            if ((timeptr = localtime(&pbl->modified_[index])) == NULL)
                item.strText = "Invalid";
            else
            {
                strftime(disp, sizeof(disp), "%c", timeptr);
                item.strText = disp;
            }
			item.lParam = pbl->modified_[index];
            break;
        case COL_ACCESSED:
			item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            if ((timeptr = localtime(&pbl->accessed_[index])) == NULL)
                item.strText = "Invalid";
            else
            {
                strftime(disp, sizeof(disp), "%c", timeptr);
                item.strText = disp;
            }
			item.lParam = pbl->accessed_[index];
            break;
		default:
			ASSERT(0);
        }

        // Make sure we don't deselect a row that was already selected
        if (column == 0 && !select && (grid_.GetItemState(row, 0) & GVIS_SELECTED) != 0)
            item.nState |= GVIS_SELECTED; 

        grid_.SetItem(&item);
    }
    grid_.RedrawRow(row);
}

/////////////////////////////////////////////////////////////////////////////
// CBookmarkDlg message handlers
void CBookmarkDlg::OnSize(UINT nType, int cx, int cy) 
{
	if (cx > 0 && m_sizeInitial.cx == -1)
		m_sizeInitial = CSize(cx, cy);
	CHexDialogBar::OnSize(nType, cx, cy);
}

void CBookmarkDlg::OnOK() 
{
    theApp.SaveToMacro(km_bookmarks, 6);
    ((CMainFrame *)AfxGetMainWnd())->ShowControlBar(this, FALSE, FALSE);
}

void CBookmarkDlg::OnDestroy() 
{
    if (grid_.m_hWnd != 0)
    {
        // Save column widths
        CString strWidths;

        for (int ii = grid_.GetFixedColumnCount(); ii < grid_.GetColumnCount(); ++ii)
        {
            CString ss;
            ss.Format("%ld,", long(grid_.GetUserColumnWidth(ii)));
            strWidths += ss;
        }

        theApp.WriteProfileString("File-Settings", "BookmarkDialogColumns", strWidths);
    }

    CHexDialogBar::OnDestroy();
}

LRESULT CBookmarkDlg::OnKickIdle(WPARAM, LPARAM lCount)
{
	ASSERT(GetDlgItem(IDOK) != NULL);
	ASSERT(GetDlgItem(IDC_BOOKMARK_NAME) != NULL);
	ASSERT(GetDlgItem(IDC_BOOKMARK_ADD) != NULL);
	ASSERT(GetDlgItem(IDC_BOOKMARK_GOTO) != NULL);
	ASSERT(GetDlgItem(IDC_BOOKMARK_REMOVE) != NULL);

    // If there are no views or no associated file disallow adding of bookmarks
    CHexEditView *pview = GetView();
	if (pview == NULL || pview->GetDocument()->pfile1_ == NULL)
    {
	    GetDlgItem(IDC_BOOKMARK_NAME)->SetWindowText("");
	    GetDlgItem(IDC_BOOKMARK_NAME)->EnableWindow(FALSE);
    }
    else
    {
	    GetDlgItem(IDC_BOOKMARK_NAME)->EnableWindow(TRUE);
    }

	CString ss;
	GetDlgItem(IDC_BOOKMARK_NAME)->GetWindowText(ss);
	GetDlgItem(IDC_BOOKMARK_ADD)->EnableWindow(!ss.IsEmpty());

    CCellRange sel = grid_.GetSelectedCellRange();
	GetDlgItem(IDC_BOOKMARK_GOTO)->EnableWindow(sel.IsValid() && sel.GetMinRow() == sel.GetMaxRow());
	GetDlgItem(IDC_BOOKMARK_REMOVE)->EnableWindow(sel.IsValid());
    return FALSE;
}

void CBookmarkDlg::OnAdd() 
{
    int index;                          // bookmark index of an existing bookmark of same name (or -1)
	CHexEditView *pview = GetView();
	ASSERT(pview != NULL);
	if (pview == NULL) return;

    CBookmarkList *pbl = theApp.GetBookmarkList();

	ASSERT(GetDlgItem(IDC_BOOKMARK_NAME) != NULL);
	CString name;
	GetDlgItem(IDC_BOOKMARK_NAME)->GetWindowText(name);

    ASSERT(name.GetLength() > 0);
    if (name[0] == '_')
	{
		AfxMessageBox("Names beginning with an underscore\r"
                      "are reserved for internal use.");
		return;
    }

	if (name.FindOneOf("|") != -1)
	{
		AfxMessageBox("Illegal characters in bookmark name");
		return;
	}

	if ((index = pbl->GetIndex(name)) != -1)
	{
		CString ss;
		ss.Format("A bookmark with the name \"%s\" already exists?\r\r"
			      "Do you want to overwrite it?", name);
		if (AfxMessageBox(ss, MB_YESNO) != IDYES)
			return;

#if 0 // moved to CBookmark
        // Remove overwritten bookmark from doc where it currently is
        CDocument *pdoc2;
        if (theApp.m_pDocTemplate->MatchDocType(pbl->file_[index], pdoc2) == 
            CDocTemplate::yesAlreadyOpen)
        {
            ((CHexEditDoc *)pdoc2)->RemoveBookmark(index);
        }
#endif
	}

    // Add the bookmark to the list (overwrites existing bookmark of same name)
	int ii = pbl->AddBookmark(name, pview->GetDocument()->pfile1_->GetFilePath(),
                              pview->GetPos(), NULL, pview->GetDocument());

    theApp.SaveToMacro(km_bookmarks_add, name);

	// Find the new/replaced bookmark and select 
	int row, col = COL_NAME + grid_.GetFixedColumnCount();
	for (row = grid_.GetFixedRowCount(); row < grid_.GetRowCount(); ++row)
	{
		if (grid_.GetItemData(row, col) == ii)
			break;
	}
	ASSERT(row < grid_.GetRowCount());  // We should have found it

	grid_.SetSelectedRange(row, grid_.GetFixedColumnCount(), row, grid_.GetColumnCount()-1);
	grid_.EnsureVisible(row, 0);

#if 0 // moved to CBookmark::Add (now calls CBookmarkDlg::UpdateBookmark)
	// Save sort col (lost when we modify the grid) so we can re-sort later
//	int sort_col = grid_.GetSortColumn();
	int row;

	if (index == -1)
	{
		// Add a new row
		row = grid_.GetRowCount();
		grid_.SetRowCount(row + 1);
	}
	else
	{
		// Find the overwritten bookmark
		int col = COL_NAME + grid_.GetFixedColumnCount();
		for (row = grid_.GetFixedRowCount(); row < grid_.GetRowCount(); ++row)
		{
			if (grid_.GetItemData(row, col) == index)
				break;
		}
		ASSERT(row < grid_.GetRowCount());  // We should have found it
	}

    // Add bookmark to doc
    pview->GetDocument()->AddBookmark(ii, pos);

	// Update the row found or added and select it
	UpdateRow(ii, row);
	grid_.SetSelectedRange(row, grid_.GetFixedColumnCount(), row, grid_.GetColumnCount()-1);
	grid_.EnsureVisible(row, 0);

	// Re-sort the grid on the current sort column
//	if (sort_col != -1)
//		grid_.SortItems(sort_col, grid_.GetSortAscending());

//	// Close dialog
//	CHexDialogBar::OnOK();
#endif
}

void CBookmarkDlg::OnGoTo() 
{
    CCellRange sel = grid_.GetSelectedCellRange();
	ASSERT(sel.IsValid() && sel.GetMinRow() == sel.GetMaxRow());

	int row = sel.GetMinRow();
	int index = grid_.GetItemData(row, COL_NAME + grid_.GetFixedColumnCount());

    CBookmarkList *pbl = theApp.GetBookmarkList();
	pbl->GoTo(index);

//	// Close the dialog
//	CHexDialogBar::OnOK();
}

void CBookmarkDlg::OnRemove() 
{
    CBookmarkList *pbl = theApp.GetBookmarkList();
    int fcc = grid_.GetFixedColumnCount();
	range_set<int> tt;                  // the grid rows to be removed
    CCellRange sel = grid_.GetSelectedCellRange();
	ASSERT(sel.IsValid());

	// Mark all selected rows for deletion
	for (int row = sel.GetMinRow(); row <= sel.GetMaxRow(); ++row)
		if (grid_.IsCellSelected(row, fcc))
		{
            int index = grid_.GetItemData(row, fcc + COL_NAME);

#if 0 // moved to pbl->RemoveBookmark()
            // Check for open document and remove from the local (doc) bookmarks as well
            CDocument *pdoc;
            if (theApp.m_pDocTemplate->MatchDocType(pbl->file_[index], pdoc) == 
                CDocTemplate::yesAlreadyOpen)
            {
                ((CHexEditDoc *)pdoc)->RemoveBookmark(index);
            }
#endif

            // Remove from the bookmark list
			pbl->Remove(index);

            // Mark for deletion from the grid (see below)
			tt.insert(row);
		}

	// Now remove the rows from the display starting from the end
	range_set<int>::const_iterator pp;

	// Remove elements from the grid starting at end so that we don't muck up the row order
	for (pp = tt.end(); pp != tt.begin(); )
        grid_.DeleteRow(*(--pp));
    grid_.Refresh();
}

void CBookmarkDlg::OnValidate() 
{
    int move_count = 0;      // Number of bookmarks moved due to being past EOF

	CWaitCursor wc;
    CBookmarkList *pbl = theApp.GetBookmarkList();
    int fcc = grid_.GetFixedColumnCount();
	range_set<int> tt;                  // the grid rows to be removed

	for (int row = grid_.GetFixedRowCount(); row < grid_.GetRowCount(); ++row)
	{
		int index = grid_.GetItemData(row, fcc + COL_NAME);
        CDocument *pdoc;
		ASSERT(index > -1 && index < int(pbl->file_.size()));

        if (theApp.m_pDocTemplate->MatchDocType(pbl->file_[index], pdoc) == 
            CDocTemplate::yesAlreadyOpen)
        {
            // Bookmarks are checked when we open the document so we don't need to validate again here
            // (Ie this bookmark was checked when the file was opened.)
#if 0
            // The file is open so we need to check that the bookmark is <= the
            // in-memory file length which may be different to the on-disk length.
            if (pbl->filepos_[index] > ((CHexEditDoc *)pdoc)->length())
            {
				TRACE2("Validate: removing %s, file %s (loaded) is too short\n", pbl->name_[index], pbl->file_[index]);
				pbl->Remove(index);
				tt.insert(row);
            }
#endif
        }
		else if (_access(pbl->file_[index], 0) != -1)
		{
			// File found but make sure the bookmark is <= file length
			CFileStatus fs;                 // Used to get file size

			if (CFile64::GetStatus(pbl->file_[index], fs) &&
				pbl->filepos_[index] > __int64(fs.m_size))
			{
				TRACE2("Validate: moving %s, file %s is too short\n", pbl->name_[index], pbl->file_[index]);
                pbl->filepos_[index] = fs.m_size;
                UpdateRow(index, row);
                ++move_count;
				// pbl->RemoveBookmark(index);
				// tt.insert(row);
			}
		}
        else
		{
			ASSERT(pbl->file_[index].Mid(1, 2) == ":\\"); // GetDriveType expects "D:\" under win9x

			// File not found so remove bookmark from the list
			// (unless it's a network/removeable file and net_retain_ is on)
			bool net_retain = ((CButton *)GetDlgItem(IDC_NET_RETAIN))->GetCheck() == BST_CHECKED;
			if (!net_retain || ::GetDriveType(pbl->file_[index].Left(3)) == DRIVE_FIXED)
			{
				TRACE2("Validate: removing bookmark %s, file %s not found\n", pbl->name_[index], pbl->file_[index]);
				pbl->RemoveBookmark(index);
				tt.insert(row);
			}
		}
	}

	// Now remove the rows from the display starting from the end
	range_set<int>::const_iterator pp;

	// Remove elements starting at end so that we don't muck up the row order
	for (pp = tt.end(); pp != tt.begin(); )
        grid_.DeleteRow(*(--pp));
    grid_.Refresh();

    if (move_count > 0 || tt.size() > 0)
    {
        CString mess;

        mess.Format("%ld bookmarks were deleted (files missing)\n"
                    "%ld bookmarks were moved (past EOF)",
                    long(tt.size()), long(move_count));
        AfxMessageBox(mess);
    }
    else
        AfxMessageBox("No bookmarks were deleted or moved.");
}

// Handlers for messages from grid control (IDC_GRID_BL)
void CBookmarkDlg::OnGridClick(NMHDR *pNotifyStruct, LRESULT* /*pResult*/)
{
    NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;
    TRACE("Left button click on row %d, col %d\n", pItem->iRow, pItem->iColumn);

    if (pItem->iRow < grid_.GetFixedRowCount())
        return;                         // Don't do anything for header rows
}

void CBookmarkDlg::OnGridDoubleClick(NMHDR *pNotifyStruct, LRESULT* /*pResult*/)
{
    NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;
    TRACE("Left button click on row %d, col %d\n", pItem->iRow, pItem->iColumn);

    if (pItem->iRow < grid_.GetFixedRowCount())
        return;                         // Don't do anything for header rows

    CCellRange sel = grid_.GetSelectedCellRange();
	if (sel.IsValid() && sel.GetMinRow() == sel.GetMaxRow())
        OnGoTo();
}

void CBookmarkDlg::OnGridRClick(NMHDR *pNotifyStruct, LRESULT* /*pResult*/)
{
    NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;
    TRACE("Right button click on row %d, col %d\n", pItem->iRow, pItem->iColumn);
    int fcc = grid_.GetFixedColumnCount();

    if (pItem->iRow < grid_.GetFixedRowCount() && pItem->iColumn >= fcc)
    {
        // Right click on column headings - create menu of columns available
        CMenu mm;
        mm.CreatePopupMenu();

        mm.AppendMenu(MF_ENABLED|(grid_.GetColumnWidth(fcc+0)>0?MF_CHECKED:0), 1, "Name");
        mm.AppendMenu(MF_ENABLED|(grid_.GetColumnWidth(fcc+1)>0?MF_CHECKED:0), 2, "File");
        mm.AppendMenu(MF_ENABLED|(grid_.GetColumnWidth(fcc+2)>0?MF_CHECKED:0), 3, "Folder");
        mm.AppendMenu(MF_ENABLED|(grid_.GetColumnWidth(fcc+3)>0?MF_CHECKED:0), 4, "Byte No");
        mm.AppendMenu(MF_ENABLED|(grid_.GetColumnWidth(fcc+4)>0?MF_CHECKED:0), 5, "Modified");
        mm.AppendMenu(MF_ENABLED|(grid_.GetColumnWidth(fcc+5)>0?MF_CHECKED:0), 6, "Accessed");

        // Work out where to display the popup menu
        CRect rct;
        grid_.GetCellRect(pItem->iRow, pItem->iColumn, &rct);
        grid_.ClientToScreen(&rct);
        int item = mm.TrackPopupMenu(
                TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                (rct.left+rct.right)/2, (rct.top+rct.bottom)/2, this);
        if (item != 0)
        {
            item += fcc-1;
            if (grid_.GetColumnWidth(item) > 0)
                grid_.SetColumnWidth(item, 0);
            else
            {
                grid_.SetColumnWidth(item, 1);
                grid_.AutoSizeColumn(item, GVS_BOTH);
            }
            grid_.ExpandColsNice(FALSE);
        }
    }
}

void CBookmarkDlg::OnHelp() 
{
    // Display help for this page
#ifdef USE_HTML_HELP
    if (!theApp.htmlhelp_file_.IsEmpty())
    {
        if (::HtmlHelp(AfxGetMainWnd()->m_hWnd, theApp.htmlhelp_file_, HH_HELP_CONTEXT, HIDD_BOOKMARKS_HELP))
            return;
    }
#endif
    if (!::WinHelp(AfxGetMainWnd()->m_hWnd, AfxGetApp()->m_pszHelpFilePath,
                   HELP_CONTEXT, HIDD_BOOKMARKS_HELP))
        ::HMessageBox(AFX_IDP_FAILED_TO_LAUNCH_HELP);
}

static DWORD id_pairs[] = { 
    IDC_BOOKMARK_NAME_DESC, HIDC_BOOKMARK_NAME,
    IDC_BOOKMARK_NAME, HIDC_BOOKMARK_NAME,
    IDC_BOOKMARK_ADD, HIDC_BOOKMARK_ADD,
    IDC_GRID_BL, HIDC_GRID_BL,
    IDC_BOOKMARK_REMOVE, HIDC_BOOKMARK_REMOVE,
    IDC_BOOKMARK_GOTO, HIDC_BOOKMARK_GOTO,
    IDC_BOOKMARKS_VALIDATE, HIDC_BOOKMARKS_VALIDATE,
    IDC_NET_RETAIN, HIDC_NET_RETAIN,
    0,0 
}; 

// This is no longer used sicen we made the dialog dockable but leave just in case
BOOL CBookmarkDlg::OnHelpInfo(HELPINFO* pHelpInfo) 
{
    ASSERT(theApp.m_pszHelpFilePath != NULL);

    CWaitCursor wait;

    if (!::WinHelp((HWND)pHelpInfo->hItemHandle, theApp.m_pszHelpFilePath, 
                   HELP_WM_HELP, (DWORD) (LPSTR) id_pairs))
        ::HMessageBox(AFX_IDP_FAILED_TO_LAUNCH_HELP);
    return TRUE;
//	return CHexDialogBar::OnHelpInfo(pHelpInfo);
}

void CBookmarkDlg::OnContextMenu(CWnd* pWnd, CPoint point) 
{
    ASSERT(theApp.m_pszHelpFilePath != NULL);

    CWaitCursor wait;

    // Don't show context menu if right-click on grid top row (used to display column menu)
    if (pWnd->IsKindOf(RUNTIME_CLASS(CGridCtrl)))
    {
        grid_.ScreenToClient(&point);
        CCellID cell = grid_.GetCellFromPt(point);
        if (cell.row == 0)
            return;
    }

    // NOTE: IMPORTANT For this to work with static text controls the control
    // must have the "Notify" (SS_NOTIFY) checkbox set.  Windows appears to
    // see that the right click was on a static text control and just pass
    // the message to the parent, which simply causes the control bars
    // context menu (IDR_CONTEXT_BARS) to be shown.
    // It took me days to work out how right click of the "Name:" static
    // text should show the "What's This" context menu.

    if (!::WinHelp((HWND)pWnd->GetSafeHwnd(), theApp.m_pszHelpFilePath, 
                   HELP_CONTEXTMENU, (DWORD) (LPSTR) id_pairs))
        ::HMessageBox(AFX_IDP_FAILED_TO_LAUNCH_HELP);
}

