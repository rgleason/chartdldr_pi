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

#include <wx/arrimpl.cpp>
    WX_DEFINE_OBJARRAY(wxArrayOfChartSources);


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
//    WMM PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "icons.h"

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

chartdldr_pi::chartdldr_pi(void *ppimgr)
      :opencpn_plugin(ppimgr)
{
      // Create the PlugIn icons
      initialize_images();
}

int chartdldr_pi::Init(void)
{
      AddLocaleCatalog( _T("opencpn-chartdldr_pi") );

      //    Get a pointer to the opencpn display canvas, to use as a parent for the POI Manager dialog
      m_parent_window = GetOCPNCanvasWindow();

      //    Get a pointer to the opencpn configuration object
      m_pconfig = GetOCPNConfigObject();

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

      //    This PlugIn needs a toolbar icon, so request its insertion if enabled locally
      if(m_bChartDldrShowIcon)
            m_leftclick_tool_id  = InsertPlugInTool(_T(""), _img_chartdldr, _img_chartdldr, wxITEM_NORMAL,
                  _("Chart Downloader"), _T(""), NULL,
                   CHARTDLDR_TOOL_POSITION, 0, this);

      return (
              WANTS_PREFERENCES         |
              WANTS_CONFIG              |
              WANTS_TOOLBAR_CALLBACK    |
              INSTALLS_TOOLBAR_TOOL
           );
}

bool chartdldr_pi::DeInit(void)
{
      m_chartSources->Clear();
      wxDELETE(m_chartSources);
      wxDELETE(m_pChartCatalog);
      wxDELETE(m_pChartSource);
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

bool chartdldr_pi::LoadConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(pConf)
      {
            pConf->SetPath ( _T ( "/Settings/ChartDnldr" ) );
            pConf->Read ( _T ( "Sources" ), &m_schartdldr_sources, _T(NOAA_CHART_SOURCES) );
            pConf->Read ( _T ( "ShowToolbarIcon" ), &m_bChartDldrShowIcon, false );
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
            m_schartdldr_sources.Append(wxString::Format(_T("%s|%s|%s|"), m_chartSources->Item(i)->GetName().c_str(), m_chartSources->Item(i)->GetUrl().c_str(), m_chartSources->Item(i)->GetDir().c_str()));
      }

      if(pConf)
      {
            pConf->SetPath ( _T ( "/Settings/ChartDnldr" ) );
            pConf->Write ( _T ( "Sources" ), m_schartdldr_sources );
            pConf->Write ( _T ( "ShowToolbarIcon" ), m_bChartDldrShowIcon );

            return true;
      }
      else
            return false;
}

void chartdldr_pi::OnToolbarToolCallback(int id)
{
      ChartDldrPrefsDialogImpl *dialog = new ChartDldrPrefsDialogImpl( m_parent_window, wxID_ANY, _("Chart Downloader"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
      dialog->m_cbShowToolbarIcon->Hide();
      dialog->pPlugIn = this;
      dialog->Fit();
      wxColour cl;
      GetGlobalColor(_T("DILG1"), &cl);
      dialog->SetBackgroundColour(cl);

      for (size_t i = 0; i < m_chartSources->GetCount(); i++)
      {
            ((wxItemContainer*)dialog->m_cbChartSources)->Append(m_chartSources->Item(i)->GetName());
      }
      dialog->m_cbShowToolbarIcon->SetValue(m_bChartDldrShowIcon);

      if(dialog->ShowModal() == wxID_OK)
      {
            m_bChartDldrShowIcon = dialog->m_cbShowToolbarIcon->IsChecked();
            SaveConfig();
      }
      dialog->Close();
      dialog->Destroy();
      wxDELETE(dialog);
}

void chartdldr_pi::ShowPreferencesDialog( wxWindow* parent )
{
      ChartDldrPrefsDialogImpl *dialog = new ChartDldrPrefsDialogImpl( parent, wxID_ANY, _("Chart Downloader"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
      dialog->pPlugIn = this;
      dialog->Fit();
      wxColour cl;
      GetGlobalColor(_T("DILG1"), &cl);
      dialog->SetBackgroundColour(cl);

      for (size_t i = 0; i < m_chartSources->GetCount(); i++)
      {
            ((wxItemContainer*)dialog->m_cbChartSources)->Append(m_chartSources->Item(i)->GetName());
      }
      dialog->m_cbShowToolbarIcon->SetValue(m_bChartDldrShowIcon);

      if(dialog->ShowModal() == wxID_OK)
      {
            m_bChartDldrShowIcon = dialog->m_cbShowToolbarIcon->IsChecked();
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

void ChartDldrPrefsDialogImpl::OnSourceSelected( wxCommandEvent& event )
{
      ChartSource *cs = pPlugIn->m_chartSources->Item(m_cbChartSources->GetSelection());
      m_tChartSourceUrl->SetValue(cs->GetUrl());
      m_dpChartDirectory->SetPath(cs->GetDir());
      
      pPlugIn->m_pChartSource = cs;
      CleanForm();
      FillFromFile(cs->GetUrl(), cs->GetDir());

      event.Skip();
}

void ChartDldrPrefsDialogImpl::CleanForm()
{
      m_tChartSourceInfo->SetValue(wxEmptyString);
      m_clCharts->DeleteAllItems();
}

void ChartDldrPrefsDialogImpl::FillFromFile(wxString url, wxString dir)
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
            m_tChartSourceInfo->SetValue(pPlugIn->m_pChartCatalog->GetDescription());
            //fill in the rest of the form
            m_clCharts->DeleteAllItems();
            for(size_t i = 0; i < pPlugIn->m_pChartCatalog->charts->Count(); i++)
            {
                  wxListItem li;
                  li.SetId(i);
                  li.SetText(pPlugIn->m_pChartCatalog->charts->Item(i).title);
                  m_clCharts->InsertItem(li);
                  m_clCharts->SetItem(i, 0, pPlugIn->m_pChartCatalog->charts->Item(i).title);
                  wxURL u(pPlugIn->m_pChartCatalog->charts->Item(i).zipfile_location);
                  wxStringTokenizer tk(u.GetPath(), _T("/"));
                  wxString file;
                  do
                  {
                        file = tk.GetNextToken();
                  } while(tk.HasMoreTokens());
                  if (!pPlugIn->m_pChartSource->ExistsLocaly(file))
                  {
                        m_clCharts->SetItem(i, 1, _("New"));
                  }
                  else
                  {
                        if(pPlugIn->m_pChartSource->IsNewerThanLocal(file, pPlugIn->m_pChartCatalog->charts->Item(i).zipfile_datetime_iso8601))
                        {
                              m_clCharts->SetItem(i, 1, _("Update available"));
                        }
                        else
                        {
                              m_clCharts->SetItem(i, 1, _("Up to date"));
                        }
                  }
            }
      }
}

bool ChartSource::ExistsLocaly(wxString filename)
{
      wxArrayString lf = GetLocalFiles();
      wxStringTokenizer tk(filename, _T("."));
      wxString file = tk.GetNextToken();
      for (size_t i = 0; i < lf.Count(); i++)
      {
            wxFileName fn(lf.Item(i));
            if(fn.GetName().StartsWith(file))
                  return true;
      }
      return false;
}

bool ChartSource::IsNewerThanLocal(wxString filename, wxDateTime validDate)
{
      wxArrayString lf = GetLocalFiles();
      wxStringTokenizer tk(filename, _T("."));
      wxString file = tk.GetNextToken();
      for (size_t i = 0; i < lf.Count(); i++)
      {
            wxFileName fn(lf.Item(i));
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

void ChartDldrPrefsDialogImpl::UpdateChartList( wxCommandEvent& event )
{
      //TODO: check if everything exists and we can write to the output dir etc.
      if (m_cbChartSources->GetSelection() < 0)
            return;
      ChartSource *cs = pPlugIn->m_chartSources->Item(m_cbChartSources->GetSelection());
      wxURL * url = new wxURL(cs->GetUrl());
      if (url->GetError() != wxURL_NOERR) 
      {
            wxMessageBox(_("Error, the URL to the chart source data seems wrong."), _("Error"));
            wxDELETE(url);
            return;
      }
      wxInputStream *in_stream;
      in_stream = url->GetInputStream();
      wxString res;
      int done = 0;
      if (url->GetError() == wxPROTO_NOERR)
      {
            wxProgressDialog prog(_("Downloading..."), _("Downloading chart list..."), in_stream->GetSize() + 1);
            prog.Show();
            wxStringOutputStream out_stream(&res);
            char * buffer = new char[8192];
            size_t read;
            do
            {
                  in_stream->Read(buffer, 8191);
                  read = in_stream->LastRead();
                  out_stream.Write(buffer, read);
                  done += read;
                  prog.Update(done);
                  
            } while (!in_stream->Eof());
            delete[] buffer;
      }
      else
      {
            wxMessageBox(_("Unable to connect."), _("Error"));
            wxDELETE(in_stream);
            wxDELETE(url);
            return;
      }
      //save
      wxStringTokenizer tk(url->GetPath(), _T("/"));
      wxString file;
      do
      {
            file = tk.GetNextToken();
      } while(tk.HasMoreTokens());
      wxFileName fn;
      fn.SetFullName(file);
      fn.SetPath(m_dpChartDirectory->GetPath());
      wxString path = fn.GetFullPath();
      if (wxFileExists(path))
            wxRemoveFile(path);
      wxTextFile txt(path);
      txt.Create();
      txt.AddLine(res);
      txt.Write();
      txt.Close();
      FillFromFile(url->GetPath(), fn.GetPath());
      //clean up
      wxDELETE(in_stream);
      wxDELETE(url);
}

wxArrayString ChartSource::GetLocalFiles()
{
      wxArrayString *ret = new wxArrayString();
      wxDir::GetAllFiles(GetDir(), ret);
      wxArrayString r(*ret);
      wxDELETE(ret);
      return r;
}

void ChartDldrPrefsDialogImpl::DownloadCharts( wxCommandEvent& event )
{
      if (m_clCharts->GetCheckedItemCount() == 0)
            return;
      int downloading = 0;
      DlProgressDialog *dialog = new DlProgressDialog(this);
      dialog->m_gTotalProgress->SetRange(m_clCharts->GetCheckedItemCount());
      dialog->Show();
      for (int i = 0; i < m_clCharts->GetItemCount(); i++)
      {
            if(m_clCharts->IsChecked(i))
            {
                  dialog->m_gTotalProgress->SetValue(downloading);
                  downloading++;
                  //download
                  wxURL * url = new wxURL(pPlugIn->m_pChartCatalog->charts->Item(i).zipfile_location);
                  if (url->GetError() != wxURL_NOERR) 
                  {
                        wxMessageBox(_("Error, the URL to the chart data seems wrong."), _("Error"));
                        wxDELETE(url);
                        dialog->Close();
                        dialog->Destroy();
                        wxDELETE(dialog);
                        return;
                  }
                  wxInputStream *in_stream;
                  in_stream = url->GetInputStream();
                  //construct local zipfile path
                  wxStringTokenizer tk(url->GetPath(), _T("/"));
                  wxString file;
                  do
                  {
                        file = tk.GetNextToken();
                  } while(tk.HasMoreTokens());
                  wxFileName fn;
                  fn.SetFullName(file);
                  fn.SetPath(m_dpChartDirectory->GetPath());
                  wxString path = fn.GetFullPath();
                  if (wxFileExists(path))
                        wxRemoveFile(path);
                  //download
                  int done = 0;
                  if (url->GetError() == wxPROTO_NOERR)
                  {
                        wxFileOutputStream out_stream(path);
                        char * buffer = new char[8192];
                        size_t read;
                        dialog->m_gChartProgress->SetValue(0);
                        dialog->m_gChartProgress->SetRange(in_stream->GetSize());
                        do
                        {
                              if(!dialog->IsShown())
                              {
                                    wxDELETE(in_stream);
                                    wxDELETE(url);
                                    dialog->Close();
                                    dialog->Destroy();
                                    wxDELETE(dialog);
                                    return;
                              }
                              in_stream->Read(buffer, 8191);
                              read = in_stream->LastRead();
                              out_stream.Write(buffer, read);
                              done += read;
                              dialog->m_gChartProgress->SetValue(done);
                        } while (!in_stream->Eof());
                        delete[] buffer;
                  }
                  else
                  {
                        wxMessageBox(_("Unable to connect."), _("Error"));
                        wxDELETE(in_stream);
                        wxDELETE(url);
                        dialog->Close();
                        dialog->Destroy();
                        wxDELETE(dialog);
                        return;
                  }
                  //unpack
                  pPlugIn->ExtractZipFiles(path, fn.GetPath(), true, pPlugIn->m_pChartCatalog->charts->Item(i).zipfile_datetime_iso8601);
                  wxRemoveFile(path);
            }
      }
      dialog->Close();
      dialog->Destroy();
      wxDELETE(dialog);
      ChartSource *cs = pPlugIn->m_chartSources->Item(m_cbChartSources->GetSelection());
      CleanForm();
      FillFromFile(cs->GetUrl(), cs->GetDir());
}

void ChartDldrPrefsDialogImpl::OnLocalDirChanged( wxFileDirPickerEvent& event )
{
      pPlugIn->m_chartSources->Item(m_cbChartSources->GetSelection())->SetDir(m_dpChartDirectory->GetPath());
      pPlugIn->SaveConfig();
      event.Skip();
}

ChartDldrPrefsDialogImpl::~ChartDldrPrefsDialogImpl()
{
      ((wxListCtrl *)m_clCharts)->DeleteAllItems();
}

ChartDldrPrefsDialogImpl::ChartDldrPrefsDialogImpl( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) 
            : ChartDldrPrefsDialog( parent, id, title, pos, size, style )
{
      // Add columns
      wxListItem col0;
      col0.SetId(0);
      col0.SetText( _("Title") );
      col0.SetWidth(200);
      ((wxListCtrl *)m_clCharts)->InsertColumn(0, col0);
      wxListItem col1;
      col1.SetId(1);
      col1.SetText( _("Status") );
      ((wxListCtrl *)m_clCharts)->InsertColumn(1, col1);
}

void ChartDldrPrefsDialogImpl::DeleteSource( wxCommandEvent& event ) 
{
      pPlugIn->m_chartSources->RemoveAt(m_cbChartSources->GetSelection());
      ((wxItemContainer*)m_cbChartSources)->Delete(m_cbChartSources->GetSelection());
      m_cbChartSources->Select(-1);
      m_tChartSourceUrl->SetValue(wxEmptyString);
      m_dpChartDirectory->SetPath(wxEmptyString);
      pPlugIn->SaveConfig();
      event.Skip(); 
}

void ChartDldrPrefsDialogImpl::AddSource( wxCommandEvent& event ) 
{
      AddSourceDlg *dialog = new AddSourceDlg(pPlugIn->m_parent_window);
      if(dialog->ShowModal() == wxID_OK)
      {
            pPlugIn->m_chartSources->Add(new ChartSource(dialog->m_tSourceName->GetValue(), dialog->m_tChartSourceUrl->GetValue(), dialog->m_dpChartDirectory->GetPath()));
            ((wxItemContainer*)m_cbChartSources)->Append(dialog->m_tSourceName->GetValue());
            /*m_cbChartSources->Select(m_cbChartSources->GetChildren().GetCount() - 1);
            m_tChartSourceUrl->SetValue(dialog->m_tChartSourceUrl->GetValue());
            m_dpChartDirectory->SetPath(dialog->m_dpChartDirectory->GetPath());*/
            pPlugIn->SaveConfig();
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
                  name = aTargetDir + wxFileName::GetPathSeparator() + name;

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
                        if (aStripPath)
                        {
                              fn.SetPath(aTargetDir);
                              name = fn.GetFullPath();
                        }

                        wxFileOutputStream file(name);

                        if (!file) {
                              wxLogError(_T("Can not create file '")+name+_T("'."));
                              ret = false;
                              break;
                        }
                        zip.Read(file);
                        fn.SetTimes(&aMTime, &aMTime, &aMTime);

                  }

            }

      } while(false);

      return ret;
}