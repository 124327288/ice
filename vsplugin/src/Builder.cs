// **********************************************************************
//
// Copyright (c) 2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// LICENSE file included in this distribution.
//
// **********************************************************************

using System;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using Extensibility;
using EnvDTE;
using EnvDTE80;
using Microsoft.VisualStudio.CommandBars;
using Microsoft.VisualStudio.VCProjectEngine;
using Microsoft.VisualStudio.VCProject;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using System.Resources;
using System.Reflection;
using VSLangProj;
using System.Globalization;
using Microsoft.VisualStudio.OLE.Interop;
using System.Runtime.InteropServices;


namespace Ice.VisualStudio
{
    public class Builder
    {
        public DTE getCurrentDTE()
        {
            return _applicationObject.DTE;
        }

        public void init(DTE2 application, AddIn addInInstance)
        {
            _applicationObject = application;
            _addInInstance = addInInstance;

            //
            // Subscribe to solution events.
            //
            _solutionEvents = application.Events.SolutionEvents;
            _solutionEvents.Opened += new _dispSolutionEvents_OpenedEventHandler(solutionOpened);
            _solutionEvents.AfterClosing += new _dispSolutionEvents_AfterClosingEventHandler(afterClosing);
            _solutionEvents.ProjectAdded += new _dispSolutionEvents_ProjectAddedEventHandler(projectAdded);
            _solutionEvents.ProjectRemoved += new _dispSolutionEvents_ProjectRemovedEventHandler(projectRemoved);
            _solutionEvents.ProjectRenamed += new _dispSolutionEvents_ProjectRenamedEventHandler(projectRenamed);

            _buildEvents = _applicationObject.Events.BuildEvents;
            _buildEvents.OnBuildBegin += new _dispBuildEvents_OnBuildBeginEventHandler(buildBegin);
            _buildEvents.OnBuildDone += new _dispBuildEvents_OnBuildDoneEventHandler(buildDone);
            foreach(Command c in _applicationObject.Commands)
            {
                if(c.Name.Equals("Project.AddNewItem"))
                {
                    _addNewItemEvent = application.Events.get_CommandEvents(c.Guid, c.ID);
                    _addNewItemEvent.AfterExecute += new _dispCommandEvents_AfterExecuteEventHandler(afterAddNewItem);
                    break;
                }
            }

            foreach(Command c in _applicationObject.Commands)
            {
                if(c.Name.Equals("Project.AddExistingItem"))
                {
                    _addExistingItemEvent = application.Events.get_CommandEvents(c.Guid, c.ID);
                    _addExistingItemEvent.AfterExecute += 
                        new _dispCommandEvents_AfterExecuteEventHandler(afterAddExistingItem);
                    break;
                }
            }
            
            //
            // Subscribe to active configuration changed.
            //
            _serviceProvider =
                new ServiceProvider((Microsoft.VisualStudio.OLE.Interop.IServiceProvider)_applicationObject.DTE);
            IVsSolutionBuildManager vsSlnBldMgr = 
                (IVsSolutionBuildManager)_serviceProvider.GetService(typeof(SVsSolutionBuildManager));
            initErrorListProvider();
            setupCommandBars();
        }

        public IVsSolution getIVsSolution()
        {
            return (IVsSolution) _serviceProvider.GetService(typeof(IVsSolution));
        }

        void buildDone(vsBuildScope Scope, vsBuildAction Action)
        {
            _building = false;
        }
        
        public bool isBuilding()
        {
            return _building;
        }
        
        private bool _building = false;

        void afterAddNewItem(string Guid, int ID, object obj, object CustomOut)
        {
            foreach(ProjectItem i in _deleted)
            {
                if(i == null)
                {
                    continue;
                }
                String fullPath = i.Properties.Item("FullPath").Value.ToString();
                if(File.Exists(fullPath))
                {
                    i.Delete();
                }
                else
                {
                    i.Remove();
                }
            }
            _deleted.Clear();
        }

        void afterAddExistingItem(string Guid, int ID, object obj, object CustomOut)
        {
            foreach(ProjectItem i in _deleted)
            {
                if(i == null)
                {
                    continue;
                }
                i.Remove();
            }
            _deleted.Clear();
        }
        
        public void disconnect()
        {
            if(_iceConfigurationCmd != null)
            {
                _iceConfigurationCmd.Delete();
            }
            
            _solutionEvents.Opened -= new _dispSolutionEvents_OpenedEventHandler(solutionOpened);
            _solutionEvents.AfterClosing -= new _dispSolutionEvents_AfterClosingEventHandler(afterClosing);
            _solutionEvents.ProjectAdded -= new _dispSolutionEvents_ProjectAddedEventHandler(projectAdded);
            _solutionEvents.ProjectRemoved -= new _dispSolutionEvents_ProjectRemovedEventHandler(projectRemoved);
            _solutionEvents.ProjectRenamed -= new _dispSolutionEvents_ProjectRenamedEventHandler(projectRenamed);
            _solutionEvents = null;

            _buildEvents.OnBuildBegin -= new _dispBuildEvents_OnBuildBeginEventHandler(buildBegin);
            _buildEvents = null;
            
            if(_dependenciesMap != null)
            {
                _dependenciesMap.Clear();
                _dependenciesMap = null;
            }

	    if(_updateList != null)
	    {
	        _updateList.Clear();
		_updateList = null;
	    }
            
            _errorCount = 0;
            if(_errors != null)
            {
                _errors.Clear();
                _errors = null;
            }
            
            if(_fileTracker != null)
            {
                _fileTracker.clear();
                _fileTracker = null;
            }
        }

        private void setupCommandBars()
        {
            CommandBar menuBarCommandBar = ((CommandBars)_applicationObject.CommandBars)["MenuBar"];

            _iceConfigurationCmd = null;
            try
            {
                _iceConfigurationCmd =
                    _applicationObject.Commands.Item(_addInInstance.ProgID + ".IceConfiguration", -1);
            }
            catch(ArgumentException)
            {
                object[] contextGUIDS = new object[] { };
                _iceConfigurationCmd = 
                    ((Commands2)_applicationObject.Commands).AddNamedCommand2(_addInInstance, 
                                "IceConfiguration",
                                "Ice Configuration...",
                                "Ice Configuration...",
                                true, -1, ref contextGUIDS,
                                (int)vsCommandStatus.vsCommandStatusSupported +
                                (int)vsCommandStatus.vsCommandStatusEnabled,
                                (int)vsCommandStyle.vsCommandStylePictAndText,
                                vsCommandControlType.vsCommandControlTypeButton);
            }

            if(_iceConfigurationCmd == null)
            {
                System.Windows.Forms.MessageBox.Show("Error initializing Ice Visual Studio Extension.\n" +
                                                     "Cannot create required commands",
                                                     "Ice Visual Studio Extension",
                                                     System.Windows.Forms.MessageBoxButtons.OK,
                                                     System.Windows.Forms.MessageBoxIcon.Error);
                return;
            }

            CommandBar toolsCmdBar = ((CommandBars)_applicationObject.CommandBars)["Tools"];
            _iceConfigurationCmd.AddControl(toolsCmdBar, toolsCmdBar.Controls.Count + 1);

            CommandBar projectCmdBar = projectCommandBar();
            _iceConfigurationCmd.AddControl(projectCmdBar, projectCmdBar.Controls.Count + 1);   
        }

        public void afterClosing()
        {
            clearErrors();
            removeDocumentEvents();
            if(_dependenciesMap != null)
            {
                _dependenciesMap.Clear();
                _dependenciesMap = null;
            }
	    if(_updateList != null)
	    {
	        _updateList.Clear();
		_updateList = null;
	    }
            trackFiles();
        }

        private void trackFiles()
        {
            if(_fileTracker == null)
            {
                return;
            }
            foreach(Project p in _applicationObject.Solution.Projects)
            {
                if(p == null)
                {
                    continue;
                }
                if(!Util.isSliceBuilderEnabled(p))
                {
                    continue;
                }
                _fileTracker.reap(p, this);
            }
        }

        public void solutionOpened()
        {
            _dependenciesMap = new Dictionary<string, Dictionary<string, List<string>>>();
	    _updateList = new List<String>();
            _fileTracker = new FileTracker();
            initDocumentEvents();
            foreach(Project p in _applicationObject.Solution.Projects)
            {
                _dependenciesMap[p.Name] = new Dictionary<string, List<string>>();
                buildProject(p, true);
            }
            if(hasErrors())
            {
                bringErrorsToFront();
            }
        }

        private void removeCppGeneratedFiles(Project project)
        {
            removeCppGeneratedItems(project.ProjectItems);
        }

        public void addBuilderToProject(Project project)
        {
            ComponentList sliceIncludes = 
                new ComponentList(Util.getProjectProperty(project, Util.PropertyNames.IceIncludePath));
            sliceIncludes.Add(Util.getIceHome(project) + "\\slice");

            Util.setProjectProperty(project, Util.PropertyNames.IceIncludePath, sliceIncludes.ToString());
            if(Util.isCppProject(project))
            {
                Util.addIceCppConfigurations(project);
                ComponentList components = new ComponentList("Ice; IceUtil");
                Util.addIceCppLibs(project, components);
                Util.setProjectProperty(project, Util.PropertyNames.Ice, true.ToString());
                buildCppProject(project, true);
            }
            else if(Util.isCSharpProject(project))
            {
                if(Util.isSilverlightProject(project))
                {
                    Util.addCSharpReference(project, "IceSL");
                }
                else
                {
                    Util.addCSharpReference(project, "Ice");
                }
                buildCSharpProject(project, true);
                Util.setProjectProperty(project, Util.PropertyNames.Ice, true.ToString());
            }
            if(hasErrors(project))
            {
                bringErrorsToFront();
            }
        }

        public void removeBuilderFromProject(Project project)
        {
            cleanProject(project);
            if(Util.isCppProject(project))
            {
                Util.removeIceCppConfigurations(project);
                Util.removeIceCppLibs(project);
                Util.setProjectProperty(project, Util.PropertyNames.Ice, false.ToString());
            }
            else if(Util.isCSharpProject(project))
            {
                if(Util.isSilverlightProject(project))
                {
                    Util.removeCSharpReference(project, "IceSL");
                }
                else
                {
                    foreach(string component in Util.ComponentNames.cSharpNames)
                    {
                        Util.removeCSharpReference(project, component);
                    }
                }
                Util.setProjectProperty(project, Util.PropertyNames.Ice, false.ToString());
            }
        }

        public void documentSaved(Document document)
        {
            Project project = null;
            try
            {
                project = document.ProjectItem.ContainingProject;
            }
            catch(COMException)
            {
                // Expected when documents are create during project initialization
                // and the ProjectItem is not yet available.
                return;
            }
            if(!Util.isSliceBuilderEnabled(project))
            {
                return;
            }
            if(!document.Name.EndsWith(".ice"))
            {
                return;
            }

            string projectDir = System.IO.Path.GetDirectoryName(project.FullName);
            _fileTracker.reap(project, this);
            clearErrors(document.FullName);

            if(Util.isCppProject(project))
            {
                buildCppProjectItem(project, document.ProjectItem, false);
            }
            else if(Util.isCSharpProject(project))
            {
                buildCSharpProjectItem(project, document.ProjectItem, false);
            }

            string relativeName = document.FullName;
            if(relativeName.ToUpper().Contains(projectDir.ToUpper()))
            {
                relativeName = Util.relativePath(projectDir, document.FullName);
            }
            Dictionary<string, List<string>> dependenciesMap = 
                new Dictionary<string,List<string>>(_dependenciesMap[project.Name]);

            //
            // Run slice custom tool in all files that depends on the saved file
            //
            foreach(KeyValuePair<string, List<string>> dependencies in dependenciesMap)
            {
                foreach(string name in dependencies.Value)
                {
                    if(String.IsNullOrEmpty(name))
                    {
                        continue;
                    }

                    if(!name.Equals(relativeName, StringComparison.CurrentCultureIgnoreCase))
                    {
                        continue;
                    }

                    ProjectItem item = Util.findItem(dependencies.Key, project.ProjectItems);
                    if(item == null)
                    {
                        continue;
                    }
                    String f = item.Properties.Item("FullPath").Value.ToString();
                    clearErrors(f);
                    if(Util.isCppProject(project))
                    {
                        buildCppProjectItem(project, item, false);
                    }
                    else if(Util.isCSharpProject(project))
                    {
                        buildCSharpProjectItem(project, item, false);
                    }
                }
            }
            if(hasErrors(project))
            {
                bringErrorsToFront();
            }
        }

        public void projectAdded(Project project)
        {
            if(Util.isSliceBuilderEnabled(project))
            {
                updateDependencies(project);
            }
        }

        public void projectRemoved(Project project)
        {
            _dependenciesMap.Remove(project.Name);
        }

        public void projectRenamed(Project project, string oldName)
        {
            _dependenciesMap.Remove(oldName);
            updateDependencies(project);
        }

        public void cleanProject(Project project)
        {
            if(project == null)
            {
                return;
            }
            clearErrors(project);
            _fileTracker.reap(project, this);

            if(Util.isCSharpProject(project))
            {
                removeCSharpGeneratedItems(project, project.ProjectItems);
            }
            else if(Util.isCppProject(project))
            {
	        _updateList.Remove(project.Name);
                removeCppGeneratedItems(project.ProjectItems);
            }
        }

        public void removeCSharpGeneratedItems(Project project, ProjectItems items)
        {
            if(project == null)
            {
                return;
            }
            if(items == null)
            {
                return;
            }

            foreach(ProjectItem i in items)
            {
                if(i == null)
                {
                    continue;
                }

                if(Util.isProjectItemFolder(i))
                {
                    removeCSharpGeneratedItems(project, i.ProjectItems);
                }
                else if(Util.isProjectItemFile(i))
                {
                    removeCSharpGeneratedItems(i);
                }
            }
        }

        public void buildProject(Project project, bool force)
        {
            if(project == null)
            {
                return;
            }

            if(!Util.isSliceBuilderEnabled(project))
            {
                return;
            }

            bool consoleOutput = Util.getProjectPropertyAsBool(project, Util.PropertyNames.ConsoleOutput);
            if(consoleOutput)
            {
                writeBuildOutput("------ Slice compilation started: Project: " + project.Name + " ------\n");
            }
            _fileTracker.reap(project, this);
            if(Util.isCSharpProject(project))
            {
                buildCSharpProject(project, force);
            }
            else if(Util.isCppProject(project))
            {
                buildCppProject(project, force);
            }
            if(consoleOutput)
            {
                if(hasErrors())
                {
                    writeBuildOutput("------ Slice compilation failed: Project: " + project.Name + " ------\n");
                }
                else
                {
                    writeBuildOutput("------ Slice compilation succeeded: Project: " + project.Name + " ------\n");
                }
            }
        }

        public void buildCppProject(Project project, bool force)
        {
            buildCppProject(project, project.ProjectItems, force);
        }

        public void buildCppProject(Project project, ProjectItems items, bool force)
        {
	    if(_updateList.Contains(project.Name))
	    {
	        _updateList.Remove(project.Name);
		if(!updateDependencies(project))
		{
		    return;
		}
	    }

            foreach(ProjectItem i in items)
            {
                if(i == null)
                {
                    continue;
                }

                if(Util.isProjectItemFilter(i))
                {
                    buildCppProject(project, i.ProjectItems, force);
                }
                else if(Util.isProjectItemFile(i))
                {
                    buildCppProjectItem(project, i, force);
                }
            }
        }

        public void buildCppProjectItem(Project project, ProjectItem item, bool force)
        {
            if(project == null)
            {
                return;
            }

            if(item == null)
            {
                return;
            }

            if(item.Name == null)
            {
                return;
            }

            if(!item.Name.EndsWith(".ice"))
            {
                return;
            }

            FileInfo iceFileInfo = new FileInfo(item.Properties.Item("FullPath").Value.ToString());
            FileInfo hFileInfo = new FileInfo(getCppGeneratedFileName(Path.GetDirectoryName(project.FullName),
                                              iceFileInfo.FullName, "h"));
            FileInfo cppFileInfo = new FileInfo(Path.ChangeExtension(hFileInfo.FullName, "cpp"));

            string output = Path.GetDirectoryName(cppFileInfo.FullName);
            buildCppProjectItem(project, output, iceFileInfo, cppFileInfo, hFileInfo, force);
        }
                
        public void buildCppProjectItem(Project project, String output, FileInfo ice, FileInfo cpp, FileInfo h, 
                                        bool force)
        {
            bool updated = false;

            if(force)
            {
                updated = true;
            }
            if(!h.Exists || !cpp.Exists)
            {
                updated = true;
            }
            else if(Util.findItem(h.FullName, project.ProjectItems) == null || 
                    Util.findItem(cpp.FullName, project.ProjectItems) == null)
            {
                updated = true;
            }
            else if(ice.LastWriteTime > h.LastWriteTime || ice.LastWriteTime > cpp.LastWriteTime)
            {
                if(!Directory.Exists(output))
                {
                    Directory.CreateDirectory(output);
                }
                updated = true;
            }
            else
            {
                //
                // Now check it any of the dependencies has changed.
                //
                string relativeName = Util.relativePath(Path.GetDirectoryName(project.FileName), ice.FullName);
                Dictionary<string, List<string>> dependenciesMap = _dependenciesMap[project.Name];
                if(dependenciesMap.ContainsKey(ice.FullName))
                {
                    List<string> fileDependencies = dependenciesMap[ice.FullName];
                    foreach(string name in fileDependencies)
                    {
                        FileInfo dependency =
                            new FileInfo(Path.Combine(Path.GetDirectoryName(project.FileName), name));
                        if(!dependency.Exists)
                        {
                            continue;
                        }
                        
                        if(dependency.LastWriteTime > cpp.LastWriteTime ||
                           dependency.LastWriteTime > h.LastWriteTime)
                        {
                            updated = true;
                            break;
                        }
                    }
                }
            }
            
            if(updated)
            {
                if(!Directory.Exists(output))
                {
                    Directory.CreateDirectory(output);
                }

                if(updateDependencies(project, null, ice.FullName, getSliceCompilerArgs(project, true)))
                {
                    if(runSliceCompiler(project, ice.FullName, output))
                    {
                        _fileTracker.trackFile(project, ice.FullName, h.FullName);
                        _fileTracker.trackFile(project, ice.FullName, cpp.FullName);
                        addCppGeneratedFiles(project, cpp, h);
                    }
                }
            }
        }
                
        public void addCppGeneratedFiles(Project project, FileInfo cpp, FileInfo h)
        {
            if(project == null)
            {
                return;
            }

            string projectDir = Path.GetDirectoryName(project.FileName);
            VCProject vcProject = (VCProject)project.Object;

            VCFile file = Util.findVCFile((IVCCollection)vcProject.Files, cpp.Name, cpp.FullName);
            
            if(file == null)
            {
                vcProject.AddFile(cpp.FullName);
            }
            
            file = Util.findVCFile((IVCCollection)vcProject.Files, h.Name, h.FullName);            
            if(file == null)
            {
                vcProject.AddFile(h.FullName);
            }
        }

        public void buildCSharpProject(Project project, bool force)
        {
            string projectDir = Path.GetDirectoryName(project.FileName);
            buildCSharpProject(project, projectDir, project.ProjectItems, force);
        }

        public void buildCSharpProject(Project project, string projectDir, ProjectItems items, bool force)
        {
            foreach(ProjectItem i in items)
            {
                if(i == null)
                {
                    continue;
                }

                if(Util.isProjectItemFolder(i))
                {
                    buildCSharpProject(project, projectDir, i.ProjectItems, force);
                }
                else if(Util.isProjectItemFile(i))
                {
                    buildCSharpProjectItem(project, i, force);
                }
            }
        }

        public String getCppGeneratedFileName(String projectDir, String fullPath, string extension)
        {
            if(String.IsNullOrEmpty(projectDir) || String.IsNullOrEmpty(fullPath))
            {
                return "";
            }
  
            if(!fullPath.EndsWith(".ice"))
            {
                return "";
            }

            if(fullPath.ToUpper().Contains(projectDir.ToUpper()))
            {
                return Path.ChangeExtension(fullPath, extension);
            }
            return Path.ChangeExtension(Path.Combine(projectDir, Path.GetFileName(fullPath)), extension);
        }

        public string getCSharpGeneratedFileName(Project project, ProjectItem item, string extension)
        {
            if(project == null)
            {
                return "";
            }

            if(item == null)
            {
                return "";
            }

            if(!item.Name.EndsWith(".ice"))
            {
                return "";
            }

            string projectDir = Path.GetDirectoryName(project.FileName);
            string generatedDir = Path.GetDirectoryName(Util.normalizePath(Util.getPathRelativeToProject(item)));
            string path = System.IO.Path.Combine(projectDir, generatedDir);
            return System.IO.Path.Combine(path, Path.ChangeExtension(item.Name, extension));
        }

        public void buildCSharpProjectItem(Project project, ProjectItem item, bool force)
        {
            if(project == null)
            {
                return;
            }

            if(item == null)
            {
                return;
            }

            if(item.Name == null)
            {
                return;
            }

            if(!item.Name.EndsWith(".ice"))
            {
                return;
            }

            FileInfo iceFileInfo = new FileInfo(item.Properties.Item("FullPath").Value.ToString());
            FileInfo generatedFileInfo = new FileInfo(getCSharpGeneratedFileName(project, item, "cs"));

            bool updated = false;
            if(force)
            {
                updated = true;
            }
            else if(!generatedFileInfo.Exists)
            {
                updated = true;
            }
            else if(iceFileInfo.LastWriteTime > generatedFileInfo.LastWriteTime)
            {
                updated = true;
            }
            else
            {
                //
                // Now check it any of the dependencies has changed.
                //
                //
                string relativeName = Util.relativePath(Path.GetDirectoryName(project.FileName), iceFileInfo.FullName);
                Dictionary<string, List<string>> dependenciesMap = _dependenciesMap[project.Name];
                List<string> fileDependencies = dependenciesMap[iceFileInfo.FullName];
                foreach(string name in fileDependencies)
                {
                    FileInfo dependency = new FileInfo(Path.Combine(Path.GetDirectoryName(project.FileName), name));
                    if(!dependency.Exists)
                    {
                        continue;
                    }

                    if(dependency.LastWriteTime > generatedFileInfo.LastWriteTime)
                    {
                        updated = true;
                        break;
                    }
                }
            }
            if(updated)
            {
                if(updateDependencies(project, item, iceFileInfo.FullName, getSliceCompilerArgs(project, true)))
                {
                    if(runSliceCompiler(project, iceFileInfo.FullName, generatedFileInfo.DirectoryName))
                    {
                        _fileTracker.trackFile(project, iceFileInfo.FullName, generatedFileInfo.FullName);
                    }

                    if(File.Exists(generatedFileInfo.FullName))
                    {
                        ProjectItem generatedItem = Util.findItem(generatedFileInfo.FullName, project.ProjectItems);
                        if(generatedItem == null)
                        {
                            project.ProjectItems.AddFromFile(generatedFileInfo.FullName);
                        }
                    }
                }
            }
        }

        private string quoteArg(string arg)
        {
            return "\"" + arg + "\"";
        }
        
        private string getSliceCompilerPath(Project project)
        {
            String compiler = Util.SliceTranslator.slice2cpp;
            if(Util.isCSharpProject(project))
            {
                if(Util.isSilverlightProject(project))
                {
                    compiler = Util.SliceTranslator.slice2sl;
                }
                else
                {
                    compiler = Util.SliceTranslator.slice2cs;
                }
            }

            String iceHome = Util.getAbsoluteIceHome(project);
            if(Directory.Exists(Path.Combine(iceHome, "cpp")))
            {
                iceHome = Path.Combine(iceHome, "cpp/bin");
            }
            else
            {
                iceHome = Path.Combine(iceHome, "bin");
            }
            return Path.Combine(iceHome, compiler);
        }

        private string getSliceCompilerArgs(Project project, bool depend)
        {
            ComponentList includes = 
                new ComponentList(Util.getProjectProperty(project, Util.PropertyNames.IceIncludePath));
            ComponentList macros = new ComponentList(Util.getProjectProperty(project, Util.PropertyNames.IceMacros));
            bool tie = Util.getProjectPropertyAsBool(project, Util.PropertyNames.IceTie);
            bool ice = Util.getProjectPropertyAsBool(project, Util.PropertyNames.IcePrefix);
            bool streaming = Util.getProjectPropertyAsBool(project, Util.PropertyNames.IceStreaming);


            string sliceCompiler = getSliceCompilerPath(project);

            string args = quoteArg(sliceCompiler) + " ";

            if(depend)
            {
                args += "--depend ";
            }
            
	    if(Util.isCppProject(project))
	    {
                String preCompiledHeader = Util.getPrecompileHeader(project);
                if(!String.IsNullOrEmpty(preCompiledHeader))
                {
                    args += "--add-header=" + preCompiledHeader + " ";
                }
	    }

            foreach(string i in includes)
            {
                if(string.IsNullOrEmpty(i))
                {
                    continue;
                }
                String include = i;
                if(include.EndsWith("\\") &&
                    include.Split(new char[]{'\\'}, StringSplitOptions.RemoveEmptyEntries).Length == 1)
                {
                    include += ".";
                }
               include = include.Replace("\\", "\\\\");
               args += "-I" + quoteArg(include) + " ";
            }



            foreach(string m in macros)
            {
                if(String.IsNullOrEmpty(m))
                {
                    continue;
                }
                args += "-D" + m + " ";
            }

            if(tie && Util.isCSharpProject(project) && !Util.isSilverlightProject(project))
            {
                args += "--tie ";
            }

            if(ice)
            {
                args += "--ice ";
            }

            if(streaming)
            {
                args += "--stream ";
            }

            return args;
        }

        public bool updateDependencies(Project project)
        {
            _dependenciesMap[project.Name] = new Dictionary<string, List<string>>();
            return updateDependencies(project, project.ProjectItems, getSliceCompilerArgs(project, true));
        }

        public void cleanDependencies(Project project, string file)
        {
            if(project == null || file == null)
            {
                return;
            }
            if(String.IsNullOrEmpty(project.Name))
            {
                return;
            }
            if(!_dependenciesMap.ContainsKey(project.Name))
            {
                return;
            }

            Dictionary<string, List<string>> projectDependencies = _dependenciesMap[project.Name];
            if(!projectDependencies.ContainsKey(file))
            {
                return;
            }
            projectDependencies.Remove(file);
            _dependenciesMap[project.Name] = projectDependencies;
        }

        public bool updateDependencies(Project project, ProjectItems items, string args)
        {
	    bool success = true;
            foreach(ProjectItem item in items)
            {
                if(item == null)
                {
                    continue;
                }

                if(Util.isProjectItemFolder(item))
                {
                    if(!updateDependencies(project, item.ProjectItems, args))
		    {
		        success = false;
		    }
                }
                else if(Util.isProjectItemFile(item))
                {
                    if(!item.Name.EndsWith(".ice"))
                    {
                        continue;
                    }

                    string fullPath = item.Properties.Item("FullPath").Value.ToString();
                    if(!updateDependencies(project, item, fullPath, args))
		    {
		        success = false;
		    }
                }
            }
	    return success;
        }

        public bool updateDependencies(Project project, ProjectItem item, string file, string args)
        {
            bool consoleOutput = Util.getProjectPropertyAsBool(project, Util.PropertyNames.ConsoleOutput);
            ProcessStartInfo processInfo;
            System.Diagnostics.Process process;

            args += quoteArg(file);
            args = "/c " + quoteArg(args);

            processInfo = new ProcessStartInfo("cmd.exe", args);
            processInfo.CreateNoWindow = true;
            processInfo.UseShellExecute = false;
            processInfo.RedirectStandardError = true;
            processInfo.RedirectStandardOutput = true;
            processInfo.WorkingDirectory = Path.GetDirectoryName(project.FileName);

            String compiler = getSliceCompilerPath(project);
            if(!File.Exists(compiler))
            {
                addError(project, file, TaskErrorCategory.Error, 0, 0, compiler +
                                            " not found. Review 'Ice Home' setting.");
                return false;
            }

            if(consoleOutput)
            {
                writeBuildOutput("cmd.exe " + args + "\n");
            }
            
            process = System.Diagnostics.Process.Start(processInfo);
            process.WaitForExit();

            if(parseErrors(project, file, process.StandardError, consoleOutput))
            {
                bringErrorsToFront();
                process.Close();
                if(Util.isCppProject(project))
		{
            	    removeCppGeneratedItems(project, file);
		}
                else if(Util.isCSharpProject(project))
		{
            	    removeCSharpGeneratedItems(item);
		}
                return false;
            }
            else
            {
                List<string> dependencies = new List<string>();
                TextReader output = process.StandardOutput;
                string line = null;

                if(!_dependenciesMap.ContainsKey(project.Name))
                {
                    _dependenciesMap[project.Name] = new Dictionary<string,List<string>>();
                }

                Dictionary<string, List<string>> projectDeps = _dependenciesMap[project.Name];
                while ((line = output.ReadLine()) != null)
                {
                    if(!String.IsNullOrEmpty(line))
                    {
                        if(line.EndsWith(" \\"))
                        {
                            line = line.Substring(0, line.Length - 2);
                        }
                        line = line.Trim();
                        if(line.EndsWith(".ice") &&
                           System.IO.Path.GetFileName(line) != System.IO.Path.GetFileName(file))
                        {
                            line = line.Replace('/', '\\');
                            dependencies.Add(line);
                        }
                    }
                }
                projectDeps[file] = dependencies;
                _dependenciesMap[project.Name] = projectDeps;
            }
            process.Close();
            return true;
        }

        public void initDocumentEvents()
        {
            //Csharp project item events.
            _csProjectItemsEvents = 
                (EnvDTE.ProjectItemsEvents)_applicationObject.Events.GetObject("CSharpProjectItemsEvents");
            if(_csProjectItemsEvents != null)
            {
                _csProjectItemsEvents.ItemAdded +=
                    new _dispProjectItemsEvents_ItemAddedEventHandler(csharpItemAdded);
                _csProjectItemsEvents.ItemRemoved +=
                    new _dispProjectItemsEvents_ItemRemovedEventHandler(csharpItemRemoved);
                _csProjectItemsEvents.ItemRenamed +=
                    new _dispProjectItemsEvents_ItemRenamedEventHandler(csharpItemRenamed);
            }

            //Cpp project item events.
            _vcProjectItemsEvents = 
                (VCProjectEngineEvents)_applicationObject.Events.GetObject("VCProjectEngineEventsObject");
            if(_vcProjectItemsEvents != null)
            {
                _vcProjectItemsEvents.ItemAdded +=
                    new _dispVCProjectEngineEvents_ItemAddedEventHandler(cppItemAdded);
                _vcProjectItemsEvents.ItemRemoved +=
                    new _dispVCProjectEngineEvents_ItemRemovedEventHandler(cppItemRemoved);
                _vcProjectItemsEvents.ItemRenamed +=
                    new _dispVCProjectEngineEvents_ItemRenamedEventHandler(cppItemRenamed);
            }

            //Visual Studio document events.
            _docEvents = _applicationObject.Events.get_DocumentEvents(null);
            if(_docEvents != null)
            {
                _docEvents.DocumentSaved += new _dispDocumentEvents_DocumentSavedEventHandler(documentSaved);
            }
        }

        public void removeDocumentEvents()
        {
            //Csharp project item events.
            if(_csProjectItemsEvents != null)
            {
                _csProjectItemsEvents.ItemAdded -= 
                    new _dispProjectItemsEvents_ItemAddedEventHandler(csharpItemAdded);
                _csProjectItemsEvents.ItemRemoved -= 
                    new _dispProjectItemsEvents_ItemRemovedEventHandler(csharpItemRemoved);
                _csProjectItemsEvents.ItemRenamed -=
                    new _dispProjectItemsEvents_ItemRenamedEventHandler(csharpItemRenamed);
                _csProjectItemsEvents = null;
            }

            //Cpp project item events.
            if(_vcProjectItemsEvents != null)
            {
                _vcProjectItemsEvents.ItemAdded -= 
                    new _dispVCProjectEngineEvents_ItemAddedEventHandler(cppItemAdded);
                _vcProjectItemsEvents.ItemRemoved -=
                    new _dispVCProjectEngineEvents_ItemRemovedEventHandler(cppItemRemoved);
                _vcProjectItemsEvents.ItemRenamed -=
                    new _dispVCProjectEngineEvents_ItemRenamedEventHandler(cppItemRenamed);
                _vcProjectItemsEvents = null;
            }

            //Visual Studio document events.
            if(_docEvents != null)
            {
                _docEvents.DocumentSaved -= new _dispDocumentEvents_DocumentSavedEventHandler(documentSaved);
                _docEvents = null;
            }
        }

        public Project getSelectedProject()
        {
            return Util.getSelectedProject(_applicationObject.DTE);
        }

        private void cppItemRenamed(object obj, object parent, string oldName)
        {
            if(obj == null)
            {
                return;
            }
            VCFile file = obj as VCFile;
            if(file == null)
            {
                return;
            }
            if(!file.Name.EndsWith(".ice"))
            {
                return;
            }
            Array projects = (Array)_applicationObject.ActiveSolutionProjects;
            if(projects == null)
            {
                return;
            }
            Project project = projects.GetValue(0) as Project;
            if(project == null)
            {
                return;
            }
            if(!Util.isSliceBuilderEnabled(project))
            {
                return;
            }
            _fileTracker.reap(project, this);
            ProjectItem item = Util.findItem(file.FullPath, project.ProjectItems);

            string fullPath = file.FullPath;
            if(Util.isCppProject(project))
            {
                string cppPath = Path.ChangeExtension(fullPath, ".cpp");
                string hPath = Path.ChangeExtension(cppPath, ".h");
                if(File.Exists(cppPath) || Util.hasItemNamed(project.ProjectItems, Path.GetFileName(cppPath)))
                {
                    System.Windows.Forms.MessageBox.Show("A file named '" + Path.GetFileName(cppPath) + 
                                                         "' already exists.\n" + "If you want to add '" + 
                                                         Path.GetFileName(fullPath) + "' first remove " + " '" + 
                                                         Path.GetFileName(cppPath) + "' and '" +
                                                         Path.GetFileName(hPath) + "' from your project.",
                                                         "Ice Visual Studio Extension",
                                                         System.Windows.Forms.MessageBoxButtons.OK,
                                                         System.Windows.Forms.MessageBoxIcon.Error);
                    item.Name = oldName;
                    return;
                }

                if(File.Exists(hPath) || Util.hasItemNamed(project.ProjectItems, Path.GetFileName(hPath)))
                {
                    System.Windows.Forms.MessageBox.Show("A file named '" + Path.GetFileName(hPath) +
                                                         "' already exists.\n" + "If you want to add '" +
                                                         Path.GetFileName(fullPath) + "' first remove " +
                                                         " '" + Path.GetFileName(cppPath) + "' and '" +
                                                         Path.GetFileName(hPath) + "' from your project.",
                                                         "Ice Visual Studio Extension",
                                                         System.Windows.Forms.MessageBoxButtons.OK,
                                                         System.Windows.Forms.MessageBoxIcon.Error);
                    item.Name = oldName;
                    return;
                }
            }

	    // Recalculate all depedndencies on a rename.
	    updateDependencies(project);
	    clearErrors(project);
	    buildProject(project, false);
        }

        private void cppItemRemoved(object obj, object parent)
        {
            if(obj == null)
            {
                return;
            }

            VCFile file = obj as VCFile;
            if(file == null)
            {
                return;
            }

            Array projects = (Array)_applicationObject.ActiveSolutionProjects;
            if(projects == null)
            {
                return;
            }
            
            if(projects.Length <= 0)
            {
                return;
            }
            Project project = projects.GetValue(0) as Project;
            if(project == null)
            {
                return;
            }
            if(!Util.isSliceBuilderEnabled(project))
            {
                return;
            }
            if(!file.Name.EndsWith(".ice"))
            {
                _fileTracker.reap(project, this);
                return;
            }
            clearErrors(file.FullPath);
            removeCppGeneratedItems(project, file.FullPath);

	    //
	    // It appears that file is not actually removed from disk at this
	    // point. Thus we need to delay dependency update until the next build.
	    //
	    if(!_updateList.Contains(project.Name))
	    {
	        _updateList.Add(project.Name);
	    }
        }

        void cppItemAdded(object obj, object parent)
        {
            if(obj == null)
            {
                return;
            }
            VCFile file = obj as VCFile;
            if(file == null)
            {
                return;
            }
            if(!file.Name.EndsWith(".ice"))
            {
                return;
            }

            string fullPath = file.FullPath;
            Array projects = (Array)_applicationObject.ActiveSolutionProjects;
            if(projects == null)
            {
                return;
            }
            if(projects.Length <= 0)
            {
                return;
            }
            Project project = projects.GetValue(0) as Project;
            if(project == null)
            {
                return;
            }
            if(!Util.isSliceBuilderEnabled(project))
            {
                return;
            }
            ProjectItem item = Util.findItem(fullPath, project.ProjectItems);
            if(item == null)
            {
                return;
            }
            if(Util.isCppProject(project))
            {
                string cppPath = getCppGeneratedFileName(Path.GetDirectoryName(project.FullName), file.FullPath, "cpp");
                string hPath = Path.ChangeExtension(cppPath, ".h");
                if(File.Exists(cppPath) || Util.hasItemNamed(project.ProjectItems, Path.GetFileName(cppPath)))
                {
                    System.Windows.Forms.MessageBox.Show("A file named '" + Path.GetFileName(cppPath) +
                                                         "' already exists.\n" + "If you want to add '" +
                                                         Path.GetFileName(fullPath) + "' first remove " +
                                                         " '" + Path.GetFileName(cppPath) + "' and '" +
                                                         Path.GetFileName(hPath) + "'.",
                                                         "Ice Visual Studio Extension",
                                                         System.Windows.Forms.MessageBoxButtons.OK,
                                                         System.Windows.Forms.MessageBoxIcon.Error);
                    _deleted.Add(item);
                    return;
                }

                if(File.Exists(hPath) || Util.hasItemNamed(project.ProjectItems, Path.GetFileName(hPath)))
                {
                    System.Windows.Forms.MessageBox.Show("A file named '" + Path.GetFileName(hPath) +
                                                         "' already exists.\n" + "If you want to add '" +
                                                         Path.GetFileName(fullPath) + "' first remove " +
                                                         " '" + Path.GetFileName(cppPath) + "' and '" +
                                                         Path.GetFileName(hPath) + "'.",
                                                         "Ice Visual Studio Extension",
                                                         System.Windows.Forms.MessageBoxButtons.OK,
                                                         System.Windows.Forms.MessageBoxIcon.Error);
                    _deleted.Add(item);
                    return;
                }
            }

	    // Recalculate all depedndencies on a add.
	    updateDependencies(project);
            clearErrors(project);
	    buildProject(project, false);
        }

        private void csharpItemRenamed(ProjectItem item, string oldName)
        {
            if(item == null || _fileTracker == null || String.IsNullOrEmpty(oldName) || item.ContainingProject == null)
            {
                return;
            }
            if(!Util.isSliceBuilderEnabled(item.ContainingProject))
            {
                return;
            }
            if(!oldName.EndsWith(".ice") || !Util.isProjectItemFile(item))
            {
                return;
            }

            //Get rid of generated files, for the .ice removed file.
            _fileTracker.reap(item.ContainingProject, this);

            string fullPath = item.Properties.Item("FullPath").Value.ToString();
            if(Util.isCSharpProject(item.ContainingProject))
            {
                string csPath = Path.ChangeExtension(fullPath, ".cs");
                if(File.Exists(csPath) || 
                   Util.hasItemNamed(item.ContainingProject.ProjectItems, Path.GetFileName(csPath)))
                {
                    System.Windows.Forms.MessageBox.Show("A file named '" + Path.GetFileName(csPath) +
                                                         "' already exists.\n" + oldName +
                                                         " could not be renamed to '" + item.Name + "'.",
                                                         "Ice Visual Studio Extension",
                                                         System.Windows.Forms.MessageBoxButtons.OK,
                                                         System.Windows.Forms.MessageBoxIcon.Error);
                    item.Name = oldName;
                    return;
                }
            }

	    // Recalculate all depedndencies on a rename.
	    updateDependencies(item.ContainingProject);
	    clearErrors(item.ContainingProject);
	    buildProject(item.ContainingProject, false);
        }

        private void csharpItemRemoved(ProjectItem item)
        {
            if(item == null || _fileTracker == null)
            {
                return;
            }
            if(String.IsNullOrEmpty(item.Name) ||  item.ContainingProject == null)
            {
                return;
            }
            if(!Util.isSliceBuilderEnabled(item.ContainingProject))
            {
                return;
            }
            if(!item.Name.EndsWith(".ice"))
            {
                return;
            }
	    string fullName = item.Properties.Item("FullPath").Value.ToString();
            clearErrors(fullName);
            removeCSharpGeneratedItems(item);

	    // Recalculate depedndencies on a remove.
	    Project project = item.ContainingProject;
	    Dictionary<string, List<string>> projectDeps = _dependenciesMap[project.Name];
	    foreach(ProjectItem i in project.ProjectItems)
	    {
		if(i.Name.EndsWith(".ice")  && i != item)
		{
		    string path = i.Properties.Item("FullPath").Value.ToString();
	            if(updateDependencies(item.ContainingProject, i, path, getSliceCompilerArgs(project, true)))
		    {
	                clearErrors(path);
	    	        buildCSharpProjectItem(item.ContainingProject, i, false);
		    }
		}
	    }
        }

        private void csharpItemAdded(ProjectItem item)
        {
            if(item == null)
            {
                return;
            }

            if(String.IsNullOrEmpty(item.Name) || item.ContainingProject == null)
            {
                return;
            }

            if(!Util.isSliceBuilderEnabled(item.ContainingProject))
            {
                return;
            }

            if(!item.Name.EndsWith(".ice"))
            {
                return;
            }

            string fullPath = item.Properties.Item("FullPath").Value.ToString();
            Project project = item.ContainingProject;
            if(project == null)
            {
                return;
            }

            String csPath = getCSharpGeneratedFileName(project, item, "cs");
            ProjectItem csItem = Util.findItem(csPath, project.ProjectItems);

            if(File.Exists(csPath) || csItem != null)
            {
                System.Windows.Forms.MessageBox.Show("A file named '" + Path.GetFileName(csPath) +
                                                     "' already exists.\n" + "If you want to add '" +
                                                     Path.GetFileName(fullPath) + "' first remove " +
                                                     " '" + Path.GetFileName(csPath) + "'.",
                                                     "Ice Visual Studio Extension",
                                                     System.Windows.Forms.MessageBoxButtons.OK,
                                                     System.Windows.Forms.MessageBoxIcon.Error);
                _deleted.Add(item);
                return;
            }

	    // Recalculate all depedndencies on a add.
	    updateDependencies(project);
            clearErrors(project);
	    buildProject(project, false);
        }

        private void removeCSharpGeneratedItems(ProjectItem item)
        {
            if(item == null)
            {
                return;
            }

            if(item.Name == null)
            {
                return;
            }

            if(!item.Name.EndsWith(".ice"))
            {
                return;
            }

            FileInfo generatedFileInfo = new FileInfo(getCSharpGeneratedFileName(item.ContainingProject, item, "cs"));
            ProjectItem generatedItem = Util.findItem(generatedFileInfo.FullName, item.ContainingProject.ProjectItems);
            if(generatedItem != null)
            {
                generatedItem.Delete();
            }
        }

        private void removeCppGeneratedItems(ProjectItems items)
        {
            foreach(ProjectItem i in items)
            {
                if(Util.isProjectItemFile(i))
                {
                    string path = i.Properties.Item("FullPath").Value.ToString();
                    if(!String.IsNullOrEmpty(path))
                    {
                        if(path.EndsWith(".ice"))
                        {
                            removeCppGeneratedItems(i);
                        }
                    }
                }
                else if(Util.isProjectItemFilter(i))
                {
                    removeCppGeneratedItems(i.ProjectItems);
                }
            }
        }

        private void removeCppGeneratedItems(ProjectItem item)
        {
            if(item == null)
            {
                return;
            }

            if(item.Name == null)
            {
                return;
            }

            if(!item.Name.EndsWith(".ice"))
            {
                return;
            }
            removeCppGeneratedItems(item.ContainingProject, item.Properties.Item("FullPath").Value.ToString());
        }

        public void removeCppGeneratedItems(Project project, String slice)
        {
            String projectDir = Path.GetDirectoryName(project.FileName);
            FileInfo hFileInfo = new FileInfo(getCppGeneratedFileName(projectDir, slice, "h"));
            FileInfo cppFileInfo = new FileInfo(Path.ChangeExtension(hFileInfo.FullName, "cpp"));

            ProjectItem generated = Util.findItem(hFileInfo.FullName, project.ProjectItems);
            if(generated != null)
            {
                if(File.Exists(hFileInfo.FullName))
                {
                    generated.Delete();
                }
                else
                {
                    generated.Remove();
                }
            }


            generated = Util.findItem(cppFileInfo.FullName, project.ProjectItems);
            if(generated != null)
            {
                if(File.Exists(hFileInfo.FullName))
                {
                    generated.Delete();
                }
                else
                {
                    generated.Remove();
                }
            }
        }


        private bool runSliceCompiler(Project project, string file, string outputDir)
        {
            
            bool consoleOutput = Util.getProjectPropertyAsBool(project, Util.PropertyNames.ConsoleOutput);
            string args = getSliceCompilerArgs(project, false);
            if(!String.IsNullOrEmpty(outputDir))
            {
                args += "--output-dir \"" + outputDir + "\" ";
            }

            args += quoteArg(file);
            args = "/c " + quoteArg(args);
            ProcessStartInfo processInfo = new ProcessStartInfo("cmd.exe", args);
            processInfo.CreateNoWindow = true;
            processInfo.UseShellExecute = false;
            processInfo.RedirectStandardError = true;
            processInfo.WorkingDirectory = System.IO.Path.GetDirectoryName(project.FileName);
            


            String compiler = getSliceCompilerPath(project);
            if(!File.Exists(compiler))
            {
                addError(project, file, TaskErrorCategory.Error, 0, 0, compiler +
                                            " not found. Review 'Ice Home' setting.");
                return false;
            }

            if(consoleOutput)
            {
                writeBuildOutput("cmd.exe " + args + "\n");
            }
            System.Diagnostics.Process process = System.Diagnostics.Process.Start(processInfo);

            process.WaitForExit();

            bool hasErrors = parseErrors(project, file, process.StandardError, consoleOutput);
            process.Close();
            if(hasErrors)
            {
                if (Util.isCppProject(project))
                {
                    removeCppGeneratedItems(project, file);
                }
                else if (Util.isCSharpProject(project))
                {
                    ProjectItem item = Util.findItem(file, project.ProjectItems);
                    if(item != null)
                    {
                        removeCSharpGeneratedItems(item);
                    }
                }
            }
            return !hasErrors;
        }

        private bool parseErrors(Project project, string file, TextReader strer, bool consoleOutput)
        {
            bool hasErrors = false;
            string errorMessage = strer.ReadLine();
            bool firstLine = true;
            while (!String.IsNullOrEmpty(errorMessage))
            {
                int i = errorMessage.IndexOf(':');
                if(i == -1)
                {
                    if(firstLine)
                    {
                        errorMessage += strer.ReadToEnd();
                        if(consoleOutput)
                        {
                            writeBuildOutput(errorMessage + "\n");
                        }
                        addError(project, "", TaskErrorCategory.Error, 1, 1, errorMessage);
                        hasErrors = true;
                        break;
                    }
                    errorMessage = strer.ReadLine();
                    continue;
                }
                if(consoleOutput)
                {
                    writeBuildOutput(errorMessage + "\n");
                }
                firstLine = false;
                i = errorMessage.IndexOf(':', i + 1);
                if(i == -1)
                {
                    errorMessage = strer.ReadLine();
                    continue;
                }
                string f = errorMessage.Substring(0, i);
                if(String.IsNullOrEmpty(f))
                {
                    errorMessage = strer.ReadLine();
                    continue;
                }

                if(!File.Exists(f))
                {
                    errorMessage = strer.ReadLine();
                    continue;
                }

                //
                // Get only errors from this file.
                //
                if(Path.GetFullPath(f.ToUpper()).Equals(Path.GetFullPath(file.ToUpper())))
                {
                    errorMessage = errorMessage.Substring(i + 1, errorMessage.Length - i - 1);
                    i = errorMessage.IndexOf(':');
                    string n = errorMessage.Substring(0, i);
                    int l;
                    try
                    {
                        l = Int16.Parse(n);
                    }
                    catch(Exception)
                    {
                        l = 0;
                    }

                    errorMessage = errorMessage.Substring(i + 1, errorMessage.Length - i - 1).Trim();
                    if(errorMessage.Equals("warning: End of input with no newline, supplemented newline"))
                    {
                        errorMessage = strer.ReadLine();
                        continue;
                    }

                    if(!String.IsNullOrEmpty(errorMessage))
                    {
                        TaskErrorCategory category = TaskErrorCategory.Error;
                        if(errorMessage.Substring(0, "warning:".Length).Equals("warning:"))
                        {
                            category = TaskErrorCategory.Warning;
                        }
                        addError(project, file, category, l, 1, errorMessage);
                        hasErrors = true;
                    }
                }
                errorMessage = strer.ReadLine();
            }
            return hasErrors;
        }

        public CommandBar projectCommandBar()
        {
            return findCommandBar(new Guid("{D309F791-903F-11D0-9EFC-00A0C911004F}"), 1026);
        }

        public CommandBar solutionCommandBar()
        {
            return findCommandBar(new Guid("{D309F791-903F-11D0-9EFC-00A0C911004F}"), 1043);
        }

        public CommandBar findCommandBar(Guid guidCmdGroup, uint menuID)
        {
            // Retrieve IVsProfferComands via DTE's IOleServiceProvider interface
            IOleServiceProvider sp = (IOleServiceProvider)_applicationObject;
            Guid guidSvc = typeof(IVsProfferCommands).GUID;
            object objService;
            sp.QueryService(ref guidSvc, ref guidSvc, out objService);
            IVsProfferCommands vsProfferCmds = (IVsProfferCommands)objService;
            return vsProfferCmds.FindCommandBar(IntPtr.Zero, ref guidCmdGroup, menuID) as CommandBar;
        }

        [ComImport,Guid("6D5140C1-7436-11CE-8034-00AA006009FA"),
         InterfaceTypeAttribute(ComInterfaceType.InterfaceIsIUnknown)]
        internal interface IOleServiceProvider 
        {
            [PreserveSig]
            int QueryService([In]ref Guid guidService, [In]ref Guid riid, 
                             [MarshalAs(UnmanagedType.Interface)] out System.Object obj);
        }

        private void buildBegin(vsBuildScope scope, vsBuildAction action)
        {
            _building = true;
            if(action == vsBuildAction.vsBuildActionBuild || action == vsBuildAction.vsBuildActionRebuildAll)
            {
                switch(scope)
                {
                    case vsBuildScope.vsBuildScopeProject:
                    {
                        Project project = getSelectedProject();
                        if(project != null)
                        {
                            if(!Util.isSliceBuilderEnabled(project))
                            {
                                break;
                            }
                            clearErrors(project);
                            if(action == vsBuildAction.vsBuildActionRebuildAll)
                            {
                                cleanProject(project);
                            }
                            buildProject(project, false);
                        }
                        if(hasErrors(project))
                        {
                            bringErrorsToFront();
                            _applicationObject.DTE.ExecuteCommand("Build.Cancel", "");
                            writeBuildOutput("------ Slice compilation contains errors. Build canceled. ------\n");
                        }
                        break;
                    }
                    default:
                    {
                        clearErrors();
                        foreach(Project p in _applicationObject.Solution.Projects)
                        {
                            if(p != null)
                            {
                                if(!Util.isSliceBuilderEnabled(p))
                                {
                                    continue;
                                }
                                if(action == vsBuildAction.vsBuildActionRebuildAll)
                                {
                                    cleanProject(p);
                                }
                                buildProject(p, false);
                            }
                        }
                        if(hasErrors())
                        {
                            bringErrorsToFront();
                            _applicationObject.DTE.ExecuteCommand("Build.Cancel", "");
                            writeBuildOutput("------ Slice compilation contains errors. Build canceled. ------\n");
                        }
                        break;
                    }
                }
            }
            else if(action == vsBuildAction.vsBuildActionClean)
            {
                switch(scope)
                {
                    case vsBuildScope.vsBuildScopeProject:
                    {
                        Project project = getSelectedProject();
                        if(project != null)
                        {
                            cleanProject(project);
                        }
                        break;
                    }
                    default:
                    {
                        foreach(Project p in _applicationObject.Solution.Projects)
                        {
                            if(p != null)
                            {
                                cleanProject(p);
                            }
                        }
                        break;
                    }
                }
            }
        }

        //
        // Initialize slice builder error list provider
        //
        private void initErrorListProvider()
        {
            _errors = new List<ErrorTask>();
            _errorListProvider = new Microsoft.VisualStudio.Shell.ErrorListProvider(_serviceProvider);
            _errorListProvider.ProviderName = "Slice Error Provider";
            _errorListProvider.ProviderGuid = new Guid("B8DA84E8-7AE3-4c71-8E43-F273A20D40D1");
            _errorListProvider.Show();
        }

        //
        // Remove all errors from slice builder error list provider
        //
        private void clearErrors()
        {
            _errorCount = 0;
            _errors.Clear();
            _errorListProvider.Tasks.Clear();
        }

        private void clearErrors(Project project)
        {
            if(project == null || _errors == null)
            {
                return;
            }

            List<ErrorTask> remove = new List<ErrorTask>();
            foreach(ErrorTask error in _errors)
            {
                if(!error.HierarchyItem.Equals(getProjectHierarchy(project)))
                {
                    continue;
                }
                if(!_errorListProvider.Tasks.Contains(error))
                {
                    continue;
                }
                remove.Add(error);
                _errorListProvider.Tasks.Remove(error);
            }

            foreach(ErrorTask error in remove)
            {
                _errors.Remove(error);
            }
        }

        private void clearErrors(String file)
        {
            if(file == null || _errors == null)
            {
                return;
            }

            List<ErrorTask> remove = new List<ErrorTask>();
            foreach(ErrorTask error in _errors)
            {
                if(error.Document.Equals(file, StringComparison.CurrentCultureIgnoreCase))
                {
                    remove.Add(error);
                    _errorListProvider.Tasks.Remove(error);
                }
            }

            foreach(ErrorTask error in remove)
            {
                _errors.Remove(error);
            }

        }
        
        private IVsHierarchy getProjectHierarchy(Project project)
        {
            IVsSolution ivSSolution = getIVsSolution();
            IVsHierarchy hierarchy = null;
            if(ivSSolution != null)
            {
                ivSSolution.GetProjectOfUniqueName(project.UniqueName, out hierarchy);
            }
            return hierarchy;
        }
        
        //
        // Add a error to slice builder error list provider.
        //
        private void addError(Project project, string file, TaskErrorCategory category, int line, int column,
                              string text)
        {
            IVsHierarchy hierarchy = getProjectHierarchy(project);

            ErrorTask errorTask = new ErrorTask();
            errorTask.ErrorCategory = category;
            // Visual Studio uses indexes starting at 0 
            // while the automation model uses indexes starting at 1
            errorTask.Line = line - 1;
            errorTask.Column = column - 1;
            if(hierarchy != null)
            {
                errorTask.HierarchyItem = hierarchy;
            }
            errorTask.Navigate += new EventHandler(errorTaskNavigate);
            errorTask.Document = file;
            errorTask.Category = TaskCategory.BuildCompile;
            errorTask.Text = text;
            _errors.Add(errorTask);
            _errorListProvider.Tasks.Add(errorTask);
            if(category == TaskErrorCategory.Error)
            {
                _errorCount++;
            }
        }

        //
        // True if there was any errors in last slice compilation.
        //
        private bool hasErrors()
        {
            return _errorCount > 0;
        }

        private bool hasErrors(Project project)
        {
            if(project == null || _errors == null)
            {
                return false;
            }

            bool errors = false;
            foreach(ErrorTask error in _errors)
            {
                if(error.HierarchyItem.Equals(getProjectHierarchy(project)))
                {
                    errors = true;
                    break;
                }
            }
            return errors;
        }

        private OutputWindowPane buildOutput()
        {
            if(_output == null)
            {
                OutputWindow window = 
                    (OutputWindow)_applicationObject.Windows.Item(EnvDTE.Constants.vsWindowKindOutput).Object;
                _output = window.OutputWindowPanes.Item("Build");
            }
            return _output;
        }

        private void writeBuildOutput(string message)
        {
            OutputWindowPane pane = buildOutput();
            if(pane == null)
            {
                return;
            }
            pane.Activate();
            pane.OutputString(message);
        }

        //
        // Force the error list to show.
        //
        private void bringErrorsToFront()
        {
            if(_errorListProvider == null)
            {
                return;
            }
            _errorListProvider.BringToFront();
            _errorListProvider.ForceShowErrors();
        }

        //
        // Navigate to a file when the error is clicked.
        //
        private void errorTaskNavigate(object sender, EventArgs e)
        {
            ErrorTask task;
            try
            {
                task = (ErrorTask)sender;
                task.Line += 1;
                _errorListProvider.Navigate(task, new Guid(EnvDTE.Constants.vsViewKindTextView));
                task.Line -= 1;
            }
            catch(Exception)
            {
            }
        }

        private DTE2 _applicationObject;
        private AddIn _addInInstance;
        private SolutionEvents _solutionEvents;
        private BuildEvents _buildEvents;
        private DocumentEvents _docEvents = null;
        private ProjectItemsEvents _csProjectItemsEvents;
        private VCProjectEngineEvents _vcProjectItemsEvents;
        private ServiceProvider _serviceProvider;

        private ErrorListProvider _errorListProvider;
        private List<ErrorTask> _errors;
        private int _errorCount = 0;
        private FileTracker _fileTracker;
        private Dictionary<string, Dictionary<string, List<string>>> _dependenciesMap;
	private List<String> _updateList;
        private OutputWindowPane _output;
        
        private CommandEvents _addNewItemEvent;
        private CommandEvents _addExistingItemEvent;
        private List<ProjectItem> _deleted = new List<ProjectItem>();
        private Command _iceConfigurationCmd;
    }
}
