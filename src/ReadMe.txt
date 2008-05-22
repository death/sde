CREATING A DEBUGGER
--------------------
The user constructs a debugger object.
The user tells the debugger of a debug client object.

LOADING A PROCESS
------------------
The user can tell the debugger to either load a new process (LoadProcess) or
to attach to a running process (AttachToProcess).
The process gets suspended.

BREAKPOINTS
------------
To add a breakpoint the user calls the AddBreakPoint method, the method
returns a breakpoint ID. The debugger calls the breakpoint's Initialize
method.

To remove a breakpoint the user calls the RemoveBreakPoint method, the method
receives a breakpoint ID. The debugger calls the breakpoint's Deinitialize
method.

To enumerate all breakpoints the user calls EnumBreakPoints, supplying a
callback function that receives the breakpoint IDs. The callback function
returns true to continue enumeration and false to stop.

To get a breakpoint the user calls GetBreakPoint with the specified
breakpoint ID.

The user can enable/disable breakpoints by calling their Enable/Disable
methods, respectively.

STARTING TO DEBUG
------------------
The user tells the debugger to start tracing by calling the Go method.

The debugger then runs the process, catching any debug event. The debugger
then calls the client's SetDebuggedThread method. If the debug event is not
an exception the debugger calls the client's methods. If the event is an 
exception, the debugger passes it to the breakpoints added so they can 
check if they were triggered. If a breakpoint was triggered, the debugger
calls the debug client's OnBreakPoint method. If no breakpoint was triggered,
the debugger calls the OnException method of the client.

The user can stop the debugger by calling its Stop method.

The user can suspend/resume the debugger by calling the Suspend/Resume
methods, respectively.
