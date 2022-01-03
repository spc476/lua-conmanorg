-- ************************************************************************
--
-- Copyright 2018 by Sean Conner.  All Rights Reserved.
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation; either version 3 of the License, or (at your
-- option) any later version.
--
-- This library is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
-- License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this library; if not, see <http://www.gnu.org/licenses/>.
--
-- Comments, questions and criticisms can be sent to: sean@conman.org
--
-- ************************************************************************
-- luacheck: ignore 611

return {
  -- --------------------------------------------------------
  -- The following are defined by Standard C <stdlib.h>
  -- --------------------------------------------------------
  
  SUCCESS     =  0, -- standard C exit code
  FAILURE     =  1, -- standard C exit code
  
  -- -----------------------------------------------------
  -- The following are defined by the LSB
  -- https://refspecs.linuxbase.org/LSB_5.0.0/LSB-Core-generic/LSB-Core-generic/iniscrptact.html
  -- -----------------------------------------------------
  
  GENERIC_UNSPECIFIED = 1, -- same as standard C exit code
  INVALIDARGUMENT     = 2, -- invalid or excess argumetns
  NOTIMPLEMENTED      = 3, -- unimplemented feature
  NOPERMISSION        = 4, -- the user has insufficient priviledges
  NOTINSTALLED        = 5, -- the program is not installed
  NOTCONFIGURED       = 6, -- the program is not configured
  NOTRUNNING          = 7, -- the program is not running
  
  -- ------------------------------------------------------------------------
  -- The following are defined by BSD, and usually found in C via <sysexits.h>
  -- ------------------------------------------------------------------------
  
  OK          =  0, -- successful termination
  USAGE       = 64, -- command line usage error
  DATAERR     = 65, -- data format error
  NOINPUT     = 66, -- cannot open input
  NOUSER      = 67, -- addressee unknown
  NOHOST      = 68, -- host name unknown
  UNAVAILABLE = 69, -- service unavailable
  SOFTWARE    = 70, -- internal software error
  OSERR       = 71, -- system error (e.g., can't fork)
  OSFILE      = 72, -- critical OS file missing
  CANTCREAT   = 73, -- can't create (user) output file
  IOERR       = 74, -- input/output error
  TEMPFAIL    = 75, -- temp failure; user is invited to retry
  PROTOCOL    = 76, -- remote error in protocol
  NOPERM      = 77, -- permission denied
  CONFIG      = 78, -- configuration error
  NOTFOUND    = 79, -- required file not found
  
  -- -------------------------------------------------------------------
  -- The following are defined by systemd
  -- https://www.freedesktop.org/software/systemd/man/systemd.exec.html
  -- -------------------------------------------------------------------
  
  CHDIR                   = 200, -- Changing to the requested working directory failed.
  NICE                    = 201, -- Failed to set up process scheduling priority (nice level).
  FDS                     = 202, -- Failed to close unwanted file descriptors, or to adjust passed file descriptors.
  EXEC                    = 203, -- The actual process execution failed (specifically, the execve(2) system call).
  MEMORY                  = 204, -- Failed to perform an action due to memory shortage.
  LIMITS                  = 205, -- Failed to adjust resource limits.
  OOM_ADJUST              = 206, -- Failed to adjust the OOM setting.
  SIGNAL_MASK             = 207, -- Failed to set process signal mask.
  STDIN                   = 208, -- Failed to set up standard input.
  STDOUT                  = 209, -- Failed to set up standard output.
  CHROOT                  = 210, -- Failed to change root directory (chroot(2)).
  IOPRIO                  = 211, -- Failed to set up IO scheduling priority.
  TIMERSLACK              = 212, -- Failed to set up timer slack.
  SECUREBITS              = 213, -- Failed to set process secure bits.
  SETSCHEDULER            = 214, -- Failed to set up CPU scheduling.
  CPUAFFINITY             = 215, -- Failed to set up CPU affinity.
  GROUP                   = 216, -- Failed to determine or change group credentials.
  USER                    = 217, -- Failed to determine or change user credentials, or to set up user namespacing.
  CAPABILITIES            = 218, -- Failed to drop capabilities, or apply ambient capabilities.
  CGROUP                  = 219, -- Setting up the service control group failed.
  SETSID                  = 220, -- Failed to create new process session.
  CONFIRM                 = 221, -- Execution has been cancelled by the user.
  STDERR                  = 222, -- Failed to set up standard error output.
  -- 223 not defined
  PAM                     = 224, -- Failed to set up PAM session.
  NETWORK                 = 225, -- Failed to set up network namespacing.
  NAMESPACE               = 226, -- Failed to set up mount, UTS, or IPC namespacing.
  NO_NEW_PRIVILEDGES      = 227, -- Failed to disable new privileges.
  SECCOMP                 = 228, -- Failed to apply system call filters.
  SELINUX_CONTEXT         = 229, -- Determining or changing SELinux context failed.
  PERSONALITY             = 230, -- Failed to set up an execution domain (personality).
  APPARMOR_PROFILE        = 231, -- Failed to prepare changing AppArmor profile.
  ADDRESS_FAMILIES        = 232, -- Failed to restrict address families.
  RUNTIME_DIRECTORY       = 233, -- Setting up runtime directory failed.
  -- 234 not defined
  CHOWN                   = 235, -- Failed to adjust socket ownership.
  SMACK_PROCESS_LABEL     = 236, -- Failed to set SMACK label.
  KEYRING                 = 237, -- Failed to set up kernel keyring.
  STATE_DIRECTORY         = 238, -- Failed to set up unit's state directory.
  CACHE_DIRECTORY         = 239, -- Failed to set up unit's cache directory.
  LOGS_DIRECTORY          = 240, -- Failed to set up unit's logging directory.
  CONFIGURATION_DIRECTORY = 241, -- Failed to set up unit's configuration directory.
  NUMA_POLICY             = 242, -- Failed to set up unit's NUMA memory policy.
  CREDENTIALS             = 243, -- Failed to set up unit's credentials.
  -- 244 not defined
  BPF                     = 245, -- Failed to apply BPF restrictions.
}
