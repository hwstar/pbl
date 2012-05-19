/*
* For pic applications using bloader, this file should be included
* in your application
*/

#define APP_ENTRY 0x400 // Entry point for app.
#define APP_ISR_ENTRY APP_ENTRY + 4 // ISR entry point
#pragma build(reset=APP_ENTRY, interrupt=APP_ISR_ENTRY) // Tell compiler to build above boot loader
#pragma org 0, APP_ENTRY-1 {} // Reserve area for boot loader

