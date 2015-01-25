/******************************************************************************
 * $Id: chartdldr_pi.cpp,v 1.0 2011/02/26 01:54:37 nohal Exp $
 *
 * Project:  OpenCPN
 * Purpose:  Chart downloader Plugin
 * Author:   Pavel Kalian
 *
 ***************************************************************************
 *   Copyright (C) 2011 by Pavel Kalian   *
 *   $EMAIL$   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "chartdldr_pi.h"
#include <wx/url.h>
#include <wx/progdlg.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/dir.h>
#include <wx/filesys.h>
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <memory>
#include <wx/protocol/http.h>

#include <wx/arrimpl.cpp>
    WX_DEFINE_OBJARRAY(wxArrayOfChartSources);
    
#ifdef __WXMAC__
#define CATALOGS_NAME_WIDTH 300
#define CATALOGS_DATE_WIDTH 120
#define CATALOGS_PATH_WIDTH 100
#define CHARTS_NAME_WIDTH 300
#define CHARTS_STATUS_WIDTH 100
#define CHARTS_DATE_WIDTH 120
#else
#define CATALOGS_NAME_WIDTH 300
#define CATALOGS_DATE_WIDTH 200
#define CATALOGS_PATH_WIDTH 300
#define CHARTS_NAME_WIDTH 400
#define CHARTS_STATUS_WIDTH 150
#define CHARTS_DATE_WIDTH 200
#endif // __WXMAC__

#define CHART_DIR "Charts"


// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new chartdldr_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

//---------------------------------------------------------------------------------------------------------
//
//    ChartDldr PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "icons.h"

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

chartdldr_pi::chartdldr_pi(void *ppimgr)
      :opencpn_plugin_112(ppimgr)
{
      // Create the PlugIn icons
      initialize_images();

      m_chartSources = NULL;
      m_parent_window = NULL;
      m_pChartCatalog = NULL;
      m_pChartSource = NULL;
      m_pconfig = NULL;
      m_leftclick_tool_id = -1;
      m_schartdldr_sources = wxEmptyString;
      wxCurlBase::Init();
}

int chartdldr_pi::Init(void)
{
      AddLocaleCatalog( _T("opencpn-chartdldr_pi") );

      //    Get a pointer to the opencpn display canvas, to use as a parent for the POI Manager dialog
      m_parent_window = GetOCPNCanvasWindow();

      //    Get a pointer to the opencpn configuration object
      m_pconfig = GetOCPNConfigObject();
      m_pOptionsPage = NULL;

      m_chartSources = new wxArrayOfChartSources();
      m_pChartCatalog = new ChartCatalog;
      m_pChartSource = NULL;

      //    And load the configuration items
      LoadConfig();

      wxStringTokenizer st(m_schartdldr_sources, _T("|"), wxTOKEN_DEFAULT);
      while ( st.HasMoreTokens() )
      {
            wxString s1 = st.GetNextToken();
            wxString s2 = st.GetNextToken();
            wxString s3 = st.GetNextToken();
            m_chartSources->Add(new ChartSource(s1, s2, s3));
      }
      return (
              WANTS_PREFERENCES         |
              WANTS_CONFIG              |
              INSTALLS_TOOLBOX_PAGE
           );
}

bool chartdldr_pi::DeInit(void)
{
      m_chartSources->Clear();
      wxDELETE(m_chartSources);
      wxDELETE(m_pChartCatalog);
      wxDELETE(m_pChartSource);
      /* TODO: Seth */
//      dialog->Close();
//      dialog->Destroy();
//      wxDELETE(dialog);
      /* We must delete remaining page if the plugin is disabled while in Options dialog */
      if ( m_pOptionsPage )
      {
            if ( DeleteOptionsPage( m_pOptionsPage ) )
                  m_pOptionsPage = NULL;
            // TODO: any other memory leak?
      }
      return true;
}

int chartdldr_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int chartdldr_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int chartdldr_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int chartdldr_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *chartdldr_pi::GetPlugInBitmap()
{
      return _img_chartdldr_pi;
}

wxString chartdldr_pi::GetCommonName()
{
      return _("ChartDownloader");
}


wxString chartdldr_pi::GetShortDescription()
{
      return _("Chart Downloader PlugIn for OpenCPN");
}

wxString chartdldr_pi::GetLongDescription()
{
      return _("Chart Downloader PlugIn for OpenCPN\n\
Manages chart downloads and updates from sources supporting\n\
NOAA Chart Catalog format");
}

void chartdldr_pi::OnSetupOptions(void)
{
      m_pOptionsPage = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("Chart Downloader") );
      if (! m_pOptionsPage) {
            wxLogMessage( _T("Error: chartdldr_pi::OnSetupOptions AddOptionsPage failed!") );
            return;
      }
      wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
      m_pOptionsPage->SetSizer( sizer );

      /* TODO: Seth */
      m_dldrpanel = new ChartDldrPanelImpl( this, m_pOptionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );

      sizer->Add( m_dldrpanel, 0, wxALL | wxEXPAND );
}

void chartdldr_pi::OnCloseToolboxPanel(int page_sel, int ok_apply_cancel)
{
      /* TODO: Seth */
      m_selected_source = m_dldrpanel->GetSelectedCatalog();
      SaveConfig();
}

bool chartdldr_pi::LoadConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(pConf)
      {
            pConf->SetPath ( _T ( "/Settings/ChartDnldr" ) );
            pConf->Read ( _T ( "ChartSources" ), &m_schartdldr_sources, wxEmptyString );
            pConf->Read ( _T ( "Source" ), &m_selected_source, -1 );
            pConf->Read ( _T ( "BaseChartDir" ), &m_base_chart_dir, *GetpPrivateApplicationDataLocation() + wxFileName::GetPathSeparator() + _T(CHART_DIR) );
            pConf->Read ( _T ( "PreselectNew" ), &m_preselect_new, false );
            pConf->Read ( _T ( "PreselectUpdated" ), &m_preselect_updated, true );
            return true;
      }
      else
            return false;
}

bool chartdldr_pi::SaveConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      m_schartdldr_sources.Clear();

      for (size_t i = 0; i < m_chartSources->GetCount(); i++)
      {
            ChartSource *cs = m_chartSources->Item(i);
            m_schartdldr_sources.Append(wxString::Format(_T("%s|%s|%s|"), cs->GetName().c_str(), cs->GetUrl().c_str(), cs->GetDir().c_str()));
      }

      if(pConf)
      {
            pConf->SetPath ( _T ( "/Settings/ChartDnldr" ) );
            pConf->Write ( _T ( "ChartSources" ), m_schartdldr_sources );
            pConf->Write ( _T ( "Source" ), m_selected_source );
            pConf->Write ( _T ( "BaseChartDir" ), m_base_chart_dir );
            pConf->Write ( _T ( "PreselectNew" ), m_preselect_new );
            pConf->Write ( _T ( "PreselectUpdated" ), m_preselect_updated );

            return true;
      }
      else
            return false;
}

void chartdldr_pi::ShowPreferencesDialog( wxWindow* parent )
{
    ChartDldrPrefsDlgImpl *dialog = new ChartDldrPrefsDlgImpl(m_parent_window);
    dialog->SetPath(m_base_chart_dir);
    dialog->SetPreferences(m_preselect_new, m_preselect_updated);
    if( wxID_OK == dialog->ShowModal() )
    {
        m_base_chart_dir = dialog->GetPath();
        dialog->GetPreferences(m_preselect_new, m_preselect_updated);
        SaveConfig();
    }
    dialog->Close();
    dialog->Destroy();
    wxDELETE(dialog);
}

ChartSource::ChartSource(wxString name, wxString url, wxString localdir)
{
      m_name = name;
      m_url = url;
      m_dir = localdir;
}

#define ID_MNU_SELALL 2001
#define ID_MNU_DELALL 2002
#define ID_MNU_INVSEL 2003
#define ID_MNU_SELUPD 2004
#define ID_MNU_SELNEW 2005

void ChartDldrPanelImpl::OnPopupClick(wxCommandEvent &evt)
{
	switch(evt.GetId()) {
		case ID_MNU_SELALL:
                  m_clCharts->CheckAll(true);
			break;
		case ID_MNU_DELALL:
                  m_clCharts->CheckAll(false);
			break;
            case ID_MNU_INVSEL:
                  for (int i = 0; i < m_clCharts->GetItemCount(); i++)
                        m_clCharts->Check(i, !m_clCharts->IsChecked(i));
			break;
            case ID_MNU_SELUPD:
            {
                  ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
                  FillFromFile(cs->GetUrl(), cs->GetDir(), false, true);
                  break;
            }
            case ID_MNU_SELNEW:
            {
                  ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
                  FillFromFile(cs->GetUrl(), cs->GetDir(), true, false);
                  break;
            }
	}
}

void ChartDldrPanelImpl::OnContextMenu( wxMouseEvent& event )
{
      if (m_clCharts->GetItemCount() == 0)
            return;
      wxMenu menu;
      wxPoint point = event.GetPosition();
      wxPoint p1 = ((wxWindow *)m_clCharts)->GetPosition();

      // add stuff
      menu.Append(ID_MNU_SELALL, _("Select all"), wxT(""));
      menu.Append(ID_MNU_DELALL, _("Deselect all"), wxT(""));
      menu.Append(ID_MNU_INVSEL, _("Invert selection"), wxT(""));
      menu.Append(ID_MNU_SELUPD, _("Select updated"), wxT(""));
      menu.Append(ID_MNU_SELNEW, _("Select new"), wxT(""));
      menu.Connect(wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&ChartDldrPanelImpl::OnPopupClick, NULL, this);
      // and then display
      PopupMenu(&menu, p1.x + point.x, p1.y + point.y);
}

void ChartDldrPanelImpl::OnShowLocalDir( wxCommandEvent& event )
{
#ifdef __WXGTK__
    wxExecute(wxString::Format(_T("xdg-open %s"), pPlugIn->m_pChartSource->GetDir().c_str()));
#endif
#ifdef __WXMAC__
    wxExecute(wxString::Format(_T("open %s"), pPlugIn->m_pChartSource->GetDir().c_str()));
#endif
#ifdef __WXMSW__
    wxExecute(wxString::Format(_T("explorer %s"), pPlugIn->m_pChartSource->GetDir().c_str()));
#endif
}

void ChartDldrPanelImpl::SetSource(int id)
{
    wxSetCursor(wxCURSOR_WAIT);
    wxYield();
    pPlugIn->SetSourceId( id );
    
    m_bDeleteSource->Enable( id >= 0 );
    m_bUpdateChartList->Enable( id >= 0 );

    CleanForm();
    if (id >= 0 && id < (int)pPlugIn->m_chartSources->Count())
    {
        ChartSource *cs = pPlugIn->m_chartSources->Item(id);
        cs->UpdateLocalFiles();
        pPlugIn->m_pChartSource = cs;
        FillFromFile(cs->GetUrl(), cs->GetDir(), pPlugIn->m_preselect_new, pPlugIn->m_preselect_updated);
    }
    else
    {
        pPlugIn->m_pChartSource = NULL;
    }
    wxSetCursor(wxCURSOR_DEFAULT);
}

void ChartDldrPanelImpl::SelectSource( wxListEvent& event )
{
    SetSource(GetSelectedCatalog());
    event.Skip();
}

void ChartDldrPanelImpl::CleanForm()
{
      m_clCharts->DeleteAllItems();
}

void ChartDldrPanelImpl::FillFromFile(wxString url, wxString dir, bool selnew, bool selupd)
{
      //load if exists
      wxStringTokenizer tk(url, _T("/"));
      wxString file;
      do
      {
            file = tk.GetNextToken();
      } while(tk.HasMoreTokens());
      wxFileName fn;
      fn.SetFullName(file);
      fn.SetPath(dir);
      wxString path = fn.GetFullPath();
      if (wxFileExists(path))
      {
            pPlugIn->m_pChartCatalog->LoadFromFile(path);
//            m_tChartSourceInfo->SetValue(pPlugIn->m_pChartCatalog->GetDescription());
            //fill in the rest of the form
            m_clCharts->DeleteAllItems();
            for(size_t i = 0; i < pPlugIn->m_pChartCatalog->charts->Count(); i++)
            {
                  wxListItem li;
                  li.SetId(i);
                  li.SetText(pPlugIn->m_pChartCatalog->charts->Item(i).GetChartTitle());
                  long x = m_clCharts->InsertItem(li);
                  m_clCharts->SetItem(x, 0, pPlugIn->m_pChartCatalog->charts->Item(i).GetChartTitle());
                  wxString file = pPlugIn->m_pChartCatalog->charts->Item(i).GetChartFilename();
                  if (!pPlugIn->m_pChartSource->ExistsLocaly(file))
                  {
                        m_clCharts->SetItem(x, 1, _("New"));
                        if (selnew)
                              m_clCharts->Check(x, true);
                  }
                  else
                  {
                        if(pPlugIn->m_pChartSource->IsNewerThanLocal(file, pPlugIn->m_pChartCatalog->charts->Item(i).GetUpdateDatetime()))
                        {
                              m_clCharts->SetItem(x, 1, _("Update available"));
                              if (selupd)
                                    m_clCharts->Check(x, true);
                        }
                        else
                        {
                              m_clCharts->SetItem(x, 1, _("Up to date"));
                        }
                  }
                  m_clCharts->SetItem(x, 2, pPlugIn->m_pChartCatalog->charts->Item(i).GetUpdateDatetime().Format(_T("%Y-%m-%d %H:%M")));
            }
      }
}

bool ChartSource::ExistsLocaly(wxString filename)
{
      wxStringTokenizer tk(filename, _T("."));
      wxString file = tk.GetNextToken().MakeLower();
      for (size_t i = 0; i < m_localfiles.Count(); i++)
      {
            wxFileName fn(m_localfiles.Item(i));
            if(fn.GetName().MakeLower() == file)
                  return true;
      }
      return false;
}

bool ChartSource::IsNewerThanLocal(wxString filename, wxDateTime validDate)
{
      wxStringTokenizer tk(filename, _T("."));
      wxString file = tk.GetNextToken();
      for (size_t i = 0; i < m_localfiles.Count(); i++)
      {
            wxFileName fn(m_localfiles.Item(i));
            if(fn.GetName().StartsWith(file))
            {
                  wxDateTime ct, mt, at;
                  fn.GetTimes(&at, &mt, &ct);
                  if (validDate.IsLaterThan(ct))
                        return true;
            }
      }
      return false;
}

int ChartDldrPanelImpl::GetSelectedCatalog()
{
    long item = m_lbChartSources->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    return item;
}

void ChartDldrPanelImpl::SelectCatalog(int item)
{
    if( item >= 0 )
    {
        m_bDeleteSource->Enable();
        m_bUpdateChartList->Enable();
    }
    else
    {
        m_bDeleteSource->Disable();
        m_bUpdateChartList->Disable();
    }
    m_lbChartSources->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void ChartDldrPanelImpl::AppendCatalog(ChartSource *cs)
{
    long id = m_lbChartSources->GetItemCount();
    m_lbChartSources->InsertItem(id, cs->GetName());
    m_lbChartSources->SetItem(id, 1, _("(Please update first)"));
    m_lbChartSources->SetItem(id, 2, cs->GetDir());
    wxURI url(cs->GetUrl());
    if (url.IsReference())
    {
        wxMessageBox(_("Error, the URL to the chart source data seems wrong."), _("Error"));
        return;
    }
    wxFileName fn(url.GetPath());
    fn.SetPath(cs->GetDir());
    wxString path = fn.GetFullPath();
    if (wxFileExists(path))
    {
        if (pPlugIn->m_pChartCatalog->LoadFromFile(path, true))
        {
            m_lbChartSources->SetItem(id, 0, pPlugIn->m_pChartCatalog->title);
            m_lbChartSources->SetItem(id, 1, pPlugIn->m_pChartCatalog->dt_valid.Format(_T("%Y-%m-%d %H:%M")));
            m_lbChartSources->SetItem(id, 2, path);
        }
    }
}

void ChartDldrPanelImpl::UpdateChartList( wxCommandEvent& event )
{
      //TODO: check if everything exists and we can write to the output dir etc.
      if (!m_lbChartSources->GetSelectedItemCount())
            return;
      ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
      wxURI url(cs->GetUrl());
      if (url.IsReference())
      {
            wxMessageBox(_("Error, the URL to the chart source data seems wrong."), _("Error"));
            return;
      }

    wxStringTokenizer tk(url.GetPath(), _T("/"));
    wxString file;
    do
    {
        file = tk.GetNextToken();
    } while(tk.HasMoreTokens());
    wxFileName fn;
    fn.SetFullName(file);
    fn.SetPath(cs->GetDir());
    if( !wxDirExists(cs->GetDir()) )
        if( !wxFileName::Mkdir(cs->GetDir(), 0755, wxPATH_MKDIR_FULL) )
        {
            wxMessageBox(wxString::Format(_("Directory %s can't be created."), cs->GetDir().c_str()), _("Chart Downloader"));
            return;
        }

    wxFileOutputStream output(fn.GetFullPath());
    wxCurlDownloadDialog ddlg(url.BuildURI(), &output, _("Downloading file"),
        _("Reading Headers: ") + url.BuildURI(), wxNullBitmap, this,
        wxCTDS_ELAPSED_TIME|wxCTDS_ESTIMATED_TIME|wxCTDS_REMAINING_TIME|wxCTDS_SPEED|wxCTDS_SIZE|wxCTDS_URL|wxCTDS_CAN_PAUSE|wxCTDS_CAN_ABORT|wxCTDS_AUTO_CLOSE);
    ddlg.SetSize(this->GetSize().GetWidth(), ddlg.GetSize().GetHeight());
    wxCurlDialogReturnFlag ret = ddlg.RunModal();
    output.Close(); 
    switch(ret)
    {
        case wxCDRF_SUCCESS:
        {
            FillFromFile(url.GetPath(), fn.GetPath());
            long id = GetSelectedCatalog();
            m_lbChartSources->SetItem(id, 0, pPlugIn->m_pChartCatalog->title);
            m_lbChartSources->SetItem(id, 1, pPlugIn->m_pChartCatalog->dt_valid.Format(_T("%Y-%m-%d %H:%M")));
            m_lbChartSources->SetItem(id, 2, cs->GetDir());
            break;
        }
        case wxCDRF_FAILED:
        {
            wxMessageBox(wxString::Format( _("Failed to Download: %s \nVerify there is a working Internet connection."), url.BuildURI().c_str() ), 
            _("Chart Downloader"), wxOK | wxICON_ERROR);
            wxRemoveFile( fn.GetFullPath() );
            break;
        }
        case wxCDRF_USER_ABORTED:
            break;
    }
}

wxArrayString ChartSource::GetLocalFiles()
{
      wxArrayString *ret = new wxArrayString();
      if( wxDirExists(GetDir()) )
        wxDir::GetAllFiles(GetDir(), ret);
      wxArrayString r(*ret);
      wxDELETE(ret);
      return r;
}

bool ChartDldrPanelImpl::DownloadChart(wxString url, wxString file, wxString title)
{
    if (cancelled)
        return false;
    downloading++;

    downloadInProgress = true;
    wxFileOutputStream output(file);
    wxCurlDownloadDialog ddlg(url, &output, wxString::Format(_("Downloading file %d of %d"), downloading, to_download),
        wxString::Format(_("Chart: %s"), title.c_str()), wxNullBitmap, this,
        wxCTDS_ELAPSED_TIME|wxCTDS_ESTIMATED_TIME|wxCTDS_REMAINING_TIME|wxCTDS_SPEED|wxCTDS_SIZE|wxCTDS_URL|wxCTDS_CAN_PAUSE|wxCTDS_CAN_ABORT|wxCTDS_AUTO_CLOSE);
    ddlg.SetSize(this->GetSize().GetWidth(), ddlg.GetSize().GetHeight());
    wxCurlDialogReturnFlag ret = ddlg.RunModal();
    output.Close(); 
    switch(ret)
    {
        case wxCDRF_SUCCESS:
            return true;
        case wxCDRF_FAILED:
        {
            wxMessageBox(wxString::Format( _("Failed to Download: %s \nVerify there is a working Internet connection."), url.c_str() ), 
            _("Chart Downloader"), wxOK | wxICON_ERROR);
            wxRemoveFile( file );
            return false;
        }
        case wxCDRF_USER_ABORTED:
            wxRemoveFile( file );
            cancelled = true;
            return false;
    }
    return false;
}

void ChartDldrPanelImpl::DownloadCharts( wxCommandEvent& event )
{
      cancelled = false;
      if (!m_lbChartSources->GetSelectedItemCount())
      {
          wxMessageBox(_("No charts selected for download."));
          return;
      }
      ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
      if (m_clCharts->GetCheckedItemCount() == 0)
      {
          wxMessageBox(_("No charts selected for download."));
          return;
      }
      to_download = m_clCharts->GetCheckedItemCount();
      downloading = 0;
      for (int i = 0; i < m_clCharts->GetItemCount(); i++)
      {
            //Prepare download queues
            if(m_clCharts->IsChecked(i))
            {
                  //download queue
                  wxURI url(pPlugIn->m_pChartCatalog->charts->Item(i).GetDownloadLocation());
                  if (url.IsReference())
                  {
                        wxMessageBox(_("Error, the URL to the chart data seems wrong."), _("Error"));
                        this->Enable();
                        return;
                  }
                  //construct local zipfile path
                  wxString file = pPlugIn->m_pChartCatalog->charts->Item(i).GetChartFilename();
                  wxFileName fn;
                  fn.SetFullName(file);
                  fn.SetPath(cs->GetDir());
                  wxString path = fn.GetFullPath();
                  if (wxFileExists(path))
                        wxRemoveFile(path);
                  wxString title = pPlugIn->m_pChartCatalog->charts->Item(i).GetChartTitle();

                  if( DownloadChart(url.BuildURI(), path, title) )
                  {
                        wxFileName fn(path);
                        pPlugIn->ExtractZipFiles(path, fn.GetPath(), true, pPlugIn->m_pChartCatalog->charts->Item(i).GetUpdateDatetime());
                        wxRemoveFile(path);
                  }
            }
      }
      SetSource(GetSelectedCatalog());
}

ChartDldrPanelImpl::~ChartDldrPanelImpl()
{
    wxCurlBase::Shutdown();
    m_lbChartSources->ClearAll();
    ((wxListCtrl *)m_clCharts)->DeleteAllItems();
}

ChartDldrPanelImpl::ChartDldrPanelImpl( chartdldr_pi* plugin, wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style )
            : ChartDldrPanel( parent, id, pos, size, style )
{
      m_bDeleteSource->Disable();
      m_bUpdateChartList->Disable();
      m_lbChartSources->InsertColumn (0, _("Catalog"), wxLIST_FORMAT_LEFT, CATALOGS_NAME_WIDTH);
      m_lbChartSources->InsertColumn (1, _("Released"), wxLIST_FORMAT_LEFT, CATALOGS_DATE_WIDTH);
      m_lbChartSources->InsertColumn (2, _("Local path"), wxLIST_FORMAT_LEFT, CATALOGS_PATH_WIDTH);

      // Add columns
      ((wxListCtrl *)m_clCharts)->InsertColumn(0, _("Title"), wxLIST_FORMAT_LEFT, CHARTS_NAME_WIDTH);
      ((wxListCtrl *)m_clCharts)->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, CHARTS_STATUS_WIDTH);
      ((wxListCtrl *)m_clCharts)->InsertColumn(2, _("Latest"), wxLIST_FORMAT_LEFT, CHARTS_DATE_WIDTH);

      downloadInProgress = false;
      cancelled = false;
      to_download = -1;
      downloading = -1;
      pPlugIn = plugin;
      m_populated = false;
}

void ChartDldrPanelImpl::OnPaint( wxPaintEvent& event )
{
    if( !m_populated )
    {
        m_populated = true;
        for (size_t i = 0; i < pPlugIn->m_chartSources->GetCount(); i++)
        {
            AppendCatalog(pPlugIn->m_chartSources->Item(i));
        }
        SelectCatalog(pPlugIn->GetSourceId());
        SetSource(pPlugIn->GetSourceId());
    }
    event.Skip();
}

void ChartDldrPanelImpl::DeleteSource( wxCommandEvent& event )
{
      if (!m_lbChartSources->GetSelectedItemCount())
            return;
      pPlugIn->m_chartSources->RemoveAt(GetSelectedCatalog());
      m_lbChartSources->DeleteItem(GetSelectedCatalog());
      CleanForm();
      pPlugIn->SetSourceId(-1);
      SelectCatalog(-1);
      pPlugIn->SaveConfig();
      event.Skip();
}

void ChartDldrPanelImpl::AddSource( wxCommandEvent& event )
{
    ChartDldrGuiAddSourceDlg *dialog = new ChartDldrGuiAddSourceDlg(this);
    dialog->SetBasePath(pPlugIn->GetBaseChartDir());
    if(dialog->ShowModal() == wxID_OK)
    {
        ChartSource *cs = new ChartSource(dialog->m_tSourceName->GetValue(), dialog->m_tChartSourceUrl->GetValue(), dialog->m_dpChartDirectory->GetTextCtrlValue());
        pPlugIn->m_chartSources->Add(cs);
        AppendCatalog(cs);

        pPlugIn->SaveConfig();
    }
    dialog->Close();
    dialog->Destroy();
    wxDELETE(dialog);
    event.Skip();
}

void ChartDldrPanelImpl::EditSource( wxCommandEvent& event )
{
    if (!m_lbChartSources->GetSelectedItemCount())
        return;
    int cat = GetSelectedCatalog();
    ChartDldrGuiAddSourceDlg *dialog = new ChartDldrGuiAddSourceDlg(this);
    dialog->SetBasePath(pPlugIn->GetBaseChartDir());
    dialog->SetSourceEdit(pPlugIn->m_chartSources->Item(cat));
    dialog->SetTitle(_("Edit Chart Source"));
    if(dialog->ShowModal() == wxID_OK)
    {
        pPlugIn->m_chartSources->Item(cat)->SetName(dialog->m_tSourceName->GetValue());
        pPlugIn->m_chartSources->Item(cat)->SetUrl(dialog->m_tChartSourceUrl->GetValue());
        pPlugIn->m_chartSources->Item(cat)->SetDir(dialog->m_dpChartDirectory->GetTextCtrlValue());

        pPlugIn->SaveConfig();
        SetSource(cat);
    }
    dialog->Close();
    dialog->Destroy();
    wxDELETE(dialog);
    event.Skip();
}

bool chartdldr_pi::ExtractZipFiles(const wxString& aZipFile, const wxString& aTargetDir, bool aStripPath, wxDateTime aMTime) {
      bool ret = true;

      std::auto_ptr<wxZipEntry> entry(new wxZipEntry());

      do {

            wxFileInputStream in(aZipFile);

            if (!in) {
                  wxLogError(_T("Can not open file '")+aZipFile+_T("'."));
                  ret = false;
                  break;
            }
            wxZipInputStream zip(in);

            while (entry.reset(zip.GetNextEntry()), entry.get() != NULL) {
                  // access meta-data
                  wxString name = entry->GetName();
                  if (aStripPath)
                  {
                      wxFileName fn(name);
                      /* We can completly replace the entry path */
                      //fn.SetPath(aTargetDir);
                      //name = fn.GetFullPath();
                      /* Or only remove the first dir (eg. ENC_ROOT) */
                      fn.RemoveDir(0);
                      name = aTargetDir + wxFileName::GetPathSeparator() + fn.GetFullPath();
                  }
                  else
                  {
                      name = aTargetDir + wxFileName::GetPathSeparator() + name;
                  }

                  // read 'zip' to access the entry's data
                  if (entry->IsDir()) {
                        int perm = entry->GetMode();
                        wxFileName::Mkdir(name, perm, wxPATH_MKDIR_FULL);
                  } else {
                        zip.OpenEntry(*entry.get());
                        if (!zip.CanRead()) {
                              wxLogError(_T("Can not read zip entry '") + entry->GetName() + _T("'."));
                              ret = false;
                              break;
                        }

                        wxFileName fn(name);
                        if (!fn.DirExists()) {
                            wxFileName::Mkdir(fn.GetPath());
                        }

                        wxFileOutputStream file(name);

                        if (!file) {
                              wxLogError(_T("Can not create file '")+name+_T("'."));
                              ret = false;
                              break;
                        }
                        zip.Read(file);
                        wxString s = aMTime.Format(_T("%Y-%m-%d %H:%M"));
                        fn.SetTimes(&aMTime, &aMTime, &aMTime);
                  }

            }

      } while(false);

      return ret;
}

ChartDldrGuiAddSourceDlg::ChartDldrGuiAddSourceDlg( wxWindow* parent ) : AddSourceDlg( parent )
{
      m_base_path = wxEmptyString;
      m_chartSources = new wxArrayOfChartSources();
      wxStringTokenizer st(_T(NOAA_CHART_SOURCES), _T("|"), wxTOKEN_DEFAULT);
      while ( st.HasMoreTokens() )
      {
            wxString s1 = st.GetNextToken();
            wxString s2 = st.GetNextToken();
            wxString s3 = st.GetNextToken();
            m_chartSources->Add(new ChartSource(s1, s2, s3));
      }
      m_rbPredefined->SetValue(true);

      for (size_t i = 0; i < m_chartSources->GetCount(); i++)
      {
            m_cbChartSources->Append(m_chartSources->Item(i)->GetName());
      }

}

ChartDldrGuiAddSourceDlg::~ChartDldrGuiAddSourceDlg()
{
      m_chartSources->Clear();
      wxDELETE(m_chartSources);
}

wxString ChartDldrGuiAddSourceDlg::FixPath(wxString path)
{
    wxString sep ( wxFileName::GetPathSeparator() );
    wxString s = path;
    s.Replace(_T("/"), sep, true);
    s.Replace(_T(USERDATA), m_base_path);
    return s;
}

void ChartDldrGuiAddSourceDlg::OnChangeType( wxCommandEvent& event )
{
      m_cbChartSources->Enable(m_rbPredefined->GetValue());
      m_tSourceName->Enable(m_rbCustom->GetValue());
      m_tChartSourceUrl->Enable(m_rbCustom->GetValue());
}

void ChartDldrGuiAddSourceDlg::OnSourceSelected( wxCommandEvent& event )
{
      ChartSource *cs = m_chartSources->Item(m_cbChartSources->GetSelection());
      m_tSourceName->SetValue(cs->GetName());
      m_tChartSourceUrl->SetValue(cs->GetUrl());
      m_dpChartDirectory->SetPath(FixPath(cs->GetDir()));

      event.Skip();
}

void ChartDldrGuiAddSourceDlg::SetSourceEdit( ChartSource* cs )
{
    m_rbCustom->SetValue(true);
    m_rbPredefined->Disable();
    m_tChartSourceUrl->Enable();
    m_cbChartSources->Disable();
    m_tSourceName->SetValue(cs->GetName());
    m_tChartSourceUrl->SetValue(cs->GetUrl());
    m_dpChartDirectory->SetPath(FixPath(cs->GetDir()));
}

ChartDldrPrefsDlgImpl::ChartDldrPrefsDlgImpl( wxWindow* parent ) : ChartDldrPrefsDlg( parent )
{
}

ChartDldrPrefsDlgImpl::~ChartDldrPrefsDlgImpl()
{    
}

void ChartDldrPrefsDlgImpl::SetPath(const wxString path)
{
    //if( !wxDirExists(path) )
        //if( !wxFileName::Mkdir(path, 0755, wxPATH_MKDIR_FULL) )
        //{
        //    wxMessageBox(wxString::Format(_("Directory %s can't be created."), m_dpDefaultDir->GetTextCtrlValue().c_str()), _("Chart Downloader"));
        //    return;
        //}
    m_dpDefaultDir->SetPath(path);
}

void ChartDldrPrefsDlgImpl::GetPreferences(bool &preselect_new, bool &preselect_updated)
{
    preselect_new = m_cbSelectNew->GetValue();
    preselect_updated = m_cbSelectUpdated->GetValue();
}
void ChartDldrPrefsDlgImpl::SetPreferences(bool preselect_new, bool preselect_updated)
{
    m_cbSelectNew->SetValue(preselect_new);
    m_cbSelectUpdated->SetValue(preselect_updated);
}

void ChartDldrGuiAddSourceDlg::OnOkClick( wxCommandEvent& event )
{
    wxString msg = wxEmptyString;
    
    if( m_rbPredefined->GetValue() && m_cbChartSources->GetSelection() < 0 )
        msg += _("You must select one of the predefined chart sources or create one of your own.\n");
    if( m_rbCustom->GetValue() && m_tSourceName->GetValue() == wxEmptyString )
        msg += _("The chart source must have a name.\n");
    wxURI url(m_tChartSourceUrl->GetValue());
    if( m_rbCustom->GetValue() && ( m_tChartSourceUrl->GetValue() == wxEmptyString || url.IsReference() ) )
        msg += _("The chart source must have a valid URL.\n");
    if( m_dpChartDirectory->GetPath() == wxEmptyString )
        msg += _("You must select a local folder to store the charts.\n");
    else
        if( !wxDirExists(m_dpChartDirectory->GetTextCtrlValue()) )
            if( !wxFileName::Mkdir(m_dpChartDirectory->GetTextCtrlValue(), 0755, wxPATH_MKDIR_FULL) )
                msg += wxString::Format(_("Directory %s can't be created."), m_dpChartDirectory->GetTextCtrlValue().c_str()) + _T("\n");
    
    if( msg != wxEmptyString )
        wxMessageBox( msg, _("Chart source definition problem"), wxOK | wxCENTRE | wxICON_ERROR );
    else
        event.Skip();
}

void ChartDldrPrefsDlgImpl::OnOkClick( wxCommandEvent& event )
{
    if( !wxDirExists(m_dpDefaultDir->GetTextCtrlValue()) )
        if( !wxFileName::Mkdir(m_dpDefaultDir->GetTextCtrlValue(), 0755, wxPATH_MKDIR_FULL) )
        {
            wxMessageBox(wxString::Format(_("Directory %s can't be created."), m_dpDefaultDir->GetTextCtrlValue().c_str()), _("Chart Downloader"));
            return;
        }
    event.Skip();
}
