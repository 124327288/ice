// **********************************************************************
//
// Copyright (c) 2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// LICENSE file included in this distribution.
//
// **********************************************************************

using System;
using System.IO;
using System.Collections.Generic;
using EnvDTE;
using System.Runtime.InteropServices;

namespace Ice.VisualStudio
{
    [ComVisible(false)]
    public class FileTracker
    {
        public FileTracker()
        {
            _files = new Dictionary<string, Dictionary<string,List<string>>>();
        }

        public void clear()
        {
            _files.Clear();
        }

        public void trackFile(Project project, String slice, String generated)
        {
            if(!_files.ContainsKey(project.Name))
            {
                _files[project.Name] = new Dictionary<string, List<string>>();
            }
            Dictionary<String, List<String>> _projectFiles = _files[project.Name];
            if(!_projectFiles.ContainsKey(slice))
            {
                _projectFiles[slice] = new List<string>();
                _projectFiles[slice].Add(generated);
            }
            else if(!_projectFiles[slice].Contains(generated))
            {
                _projectFiles[slice].Add(generated);
            }
        }

        public void reap(Project project, Builder builder)
        {
            lock(this)
            {
                if(_reaping)
                {
                    return;
                }
                _reaping = true;
            }
        
            try
            {
                if(project == null)
                {
                    return;
                }

                if(!_files.ContainsKey(project.Name))
                {
                    return;
                }

                Dictionary<string, List<string>> projectFiles = _files[project.Name];
                List<String> removedSlice = new List<String>();
                foreach(KeyValuePair<string, List<string>> i in projectFiles)
                {
                    ProjectItem item = Util.findItem(i.Key, project.ProjectItems);
                    if(item == null) //Slice file not longer in the project.
                    {
                        //Remove generated files for the slice.
                        List<String> removedFiles = new List<string>();
                        removedSlice.Add(i.Key);
                        List<ProjectItem> generated = new List<ProjectItem>();
                        foreach(string f in i.Value)
                        {
                            ProjectItem generatedItem = Util.findItem(f, project.ProjectItems);
                            if(generatedItem == null)
                            {
                                continue;
                            }
                            generated.Add(generatedItem);
                            removedFiles.Add(f);
                        }

                        foreach(ProjectItem generatedItem in generated)
                        {
                            if(generatedItem == null)
                            {
                                continue;
                            }
                            generatedItem.Delete();
                        }
                        foreach(String f in removedFiles)
                        {
                            if(File.Exists(f))
                            {
                                try
                                {
                                    File.Delete(f);
                                }
                                catch(IOException)
                                {
                                }
                            }
                        }
                    }
                }

                foreach (String slice in removedSlice)
                {
                    if (String.IsNullOrEmpty(slice))
                    {
                        continue;
                    }
                    projectFiles.Remove(slice);
                }
            }
            catch(Exception)
            {
            }
            finally
            {
                lock(this)
                {
                    _reaping = false;
                }
            }
        }

        private Dictionary<string, Dictionary<string, List<string>>> _files;
        private bool _reaping = false;
    }
}
