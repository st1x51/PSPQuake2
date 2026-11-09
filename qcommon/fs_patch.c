/* Kingpin developer snapshot commands, ported from E:/Kingpin/src/qcommon/files.c.
 * Uses rootdir and portable file APIs; commands only write inventories. The
 * generated deleted.bat is a compatibility artifact and is never executed.
 * GPL-2.0-or-later, as the source filesystem implementation.
 */
#include "qcommon.h"
#include <sys/stat.h>

#ifdef KINGPIN_ENGINE
static cvar_t *fs_patch_snapshotfile;
#define FS_PATCH_MAX_FILES 20000

typedef struct
{
	char	path[1024];
	long	mtime;
	int		size;
	int		checksum;
} patch_snapshot_entry_t;

void SCR_UpdateScreen (void);

/* PSP enumeration uses real file attributes, including extensionless files. */
static qboolean FS_PatchIsFile (const char *path)
{
	struct stat info;
	return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static const char *FS_PatchBaseName (const char *path)
{
	const char *name;

	name = strrchr (path, '/');
	return name ? name + 1 : path;
}

static const char *FS_PatchRelativeName (const char *path)
{
	const char *root = FS_GetWriteDir(FS_PATH_ROOTDIR);
	size_t length = strlen(root);
	if (!strncmp(path, root, length) && path[length] == '/')
		return path + length + 1;
	return path;
}

static FILE *FS_PatchOpen (const char *relative, const char *mode)
{
	char path[MAX_OSPATH];
	Com_sprintf(path, sizeof(path), "%s/%s", FS_GetWriteDir(FS_PATH_ROOTDIR), relative);
	return fopen(path, mode);
}

static void FS_FreeFileList (char **files, int count)
{
	int i;

	if (!files)
		return;
	for (i = 0; i < count - 1; i++)
		free (files[i]);
	free (files);
}

static qboolean FS_PatchFileInfo (const char *relative, long *mtime, int *size)
{
	char path[MAX_OSPATH];
	struct stat info;
	Com_sprintf(path, sizeof(path), "%s/%s", FS_GetWriteDir(FS_PATH_ROOTDIR), relative);
	if (stat(path, &info) < 0 || !S_ISREG(info.st_mode))
		return false;
	if (mtime) *mtime = (long)info.st_mtime;
	if (size) *size = (int)info.st_size;
	return true;
}

static qboolean FS_PatchChecksum (const char *relative, unsigned *checksum)
{
	FILE *file = FS_PatchOpen (relative, "rb");
	int success;
	if (!file)
		return false;
	*checksum = Com_StreamChecksum (file, &success);
	fclose (file);
	return success;
}

static void FS_WritePatchSnapshotTree (FILE *snapshot, const char *path)
{
	char findname[1024];
	char child[1024];
	char **files;
	const char *relative;
	unsigned checksum;
	long mtime;
	int size;
	int count;
	int i;

	Com_sprintf (findname, sizeof(findname), "%s/*", path);
	files = FS_ListFiles (findname, &count, 0, 0);
	if (!files)
		return;

	/* Directories first, exactly like 00424140. */
	for (i = 0; i < count - 1; i++)
	{
		if (FS_PatchIsFile(files[i]))
			continue;
		Com_sprintf (child, sizeof(child), "%s/%s", path,
			FS_PatchBaseName(files[i]));
		FS_WritePatchSnapshotTree (snapshot, child);
	}

	checksum = 0;
	for (i = 0; i < count - 1; i++)
	{
		if (!FS_PatchIsFile(files[i]))
			continue;

		relative = FS_PatchRelativeName (files[i]);
		if (!FS_PatchChecksum (relative, &checksum))
			continue;
		if (FS_PatchFileInfo(relative, &mtime, &size))
			fprintf (snapshot, "\"%s\" %ld %i %i\n", relative,
				mtime, size, (int)checksum);
	}

	FS_FreeFileList (files, count);
	Com_Printf (".");
	SCR_UpdateScreen ();
}

static void FS_PatchSnapshot_f (void)
{
	FILE *snapshot;
	char tree[MAX_OSPATH];

	snapshot = FS_PatchOpen (fs_patch_snapshotfile->string, "w");
	if (!snapshot)
	{
		Com_Printf ("Unable to open %s for writing\n",
			fs_patch_snapshotfile->string);
		return;
	}

	Com_Printf ("Taking snapshot of current files to: %s\n\n"
		"This may take a few minutes.\n\n", fs_patch_snapshotfile->string);
	Com_sprintf(tree, sizeof(tree), "%s/" BASEDIRNAME, FS_GetWriteDir(FS_PATH_ROOTDIR));
	FS_WritePatchSnapshotTree (snapshot, tree);
	Com_Printf ("\ndone.\n");
	fclose (snapshot);
}

static patch_snapshot_entry_t **FS_ReadPatchSnapshot (FILE *snapshot,
	int *num_entries)
{
	patch_snapshot_entry_t **entries;
	patch_snapshot_entry_t *entry;
	char line[1400];
	unsigned checksum;
	long current_mtime;
	int current_size;
	int total;
	int parsed;

	entries = malloc (FS_PATCH_MAX_FILES * sizeof(*entries));
	if (!entries)
		Com_Error (ERR_FATAL, "FS_ReadPatchSnapshot: out of memory");
	memset (entries, 0, FS_PATCH_MAX_FILES * sizeof(*entries));

	total = 0;
	*num_entries = 0;
	while (fgets(line, sizeof(line), snapshot) != NULL)
	{
		entry = malloc (sizeof(*entry));
		if (!entry)
			Com_Error (ERR_FATAL, "FS_ReadPatchSnapshot: out of memory");
		memset (entry, 0, sizeof(*entry));

		parsed = sscanf (line, "\"%1023[^\"]\" %ld %i %i",
			entry->path, &entry->mtime, &entry->size, &entry->checksum);
		if (parsed != 4)
		{
			free (entry);
			continue;
		}

		if (!(total % 100))
		{
			Com_Printf (".");
			SCR_UpdateScreen ();
		}
		total++;

		/* Missing files remain in the list and become deleted.bat entries. */
		if (FS_PatchFileInfo(entry->path, &current_mtime, &current_size))
		{
			if (entry->size != current_size)
			{
				Com_Printf ("\n%s", entry->path);
				free (entry);
				continue;
			}

			if (FS_PatchChecksum(entry->path, &checksum) &&
				(int)checksum != entry->checksum)
			{
				Com_Printf ("\n%s", entry->path);
				free (entry);
				continue;
			}
		}

		if (*num_entries == FS_PATCH_MAX_FILES)
		{
			free (entry);
			Com_Printf ("\nSnapshot contains more than %i files\n",
				FS_PATCH_MAX_FILES);
			break;
		}
		entries[(*num_entries)++] = entry;
	}

	return entries;
}

static void FS_ScanPatchTree (FILE *patch, const char *path,
	patch_snapshot_entry_t **entries, int num_entries)
{
	char findname[1024];
	char child[1024];
	char **files;
	const char *relative;
	int count;
	int i;
	int j;

	Com_sprintf (findname, sizeof(findname), "%s/*", path);
	files = FS_ListFiles (findname, &count, 0, 0);
	if (!files)
		return;

	for (i = 0; i < count - 1; i++)
	{
		if (FS_PatchIsFile(files[i]))
			continue;
		Com_sprintf (child, sizeof(child), "%s/%s", path,
			FS_PatchBaseName(files[i]));
		FS_ScanPatchTree (patch, child, entries, num_entries);
	}

	for (i = 0; i < count - 1; i++)
	{
		if (!FS_PatchIsFile(files[i]))
			continue;

		relative = FS_PatchRelativeName (files[i]);
		for (j = 0; j < num_entries; j++)
		{
			if (entries[j] && !strcmp(relative, entries[j]->path))
			{
				free (entries[j]);
				entries[j] = NULL;
				break;
			}
		}

		if (j == num_entries)
		{
			fprintf (patch, "%s\n", relative);
			Com_Printf ("\nAdding %s.", relative);
			SCR_UpdateScreen ();
		}
	}

	FS_FreeFileList (files, count);
	Com_Printf (".");
	SCR_UpdateScreen ();
}

static void FS_FreePatchEntries (patch_snapshot_entry_t **entries,
	int num_entries)
{
	int i;

	if (!entries)
		return;
	for (i = 0; i < num_entries; i++)
		free (entries[i]);
	free (entries);
}

static void FS_PatchCreate_f (void)
{
	patch_snapshot_entry_t **entries;
	FILE *snapshot;
	FILE *patch;
	FILE *deleted;
	int num_entries;
	int i;
	char tree[MAX_OSPATH];

	snapshot = FS_PatchOpen (fs_patch_snapshotfile->string, "r");
	if (!snapshot)
	{
		Com_Printf ("Unable to open %s for reading\n",
			fs_patch_snapshotfile->string);
		return;
	}

	Com_Printf ("Creating patch file listing..\n");
	Com_Printf ("Reading snapshot..");
	entries = FS_ReadPatchSnapshot (snapshot, &num_entries);
	Com_Printf ("\n\ndone.\n\n");
	fclose (snapshot);

	patch = FS_PatchOpen ("patch.txt", "w");
	if (!patch)
	{
		Com_Printf ("Unable to open %s for reading\n", "patch.txt");
		FS_FreePatchEntries (entries, num_entries);
		return;
	}
	fprintf (patch, "leave_this_line\n");
	Com_Printf ("Looking for new/changed files..");
	Com_sprintf(tree, sizeof(tree), "%s/" BASEDIRNAME, FS_GetWriteDir(FS_PATH_ROOTDIR));
	FS_ScanPatchTree (patch, tree, entries, num_entries);
	Com_Printf ("\n\ndone.\n\n");
	fclose (patch);

	Com_Printf ("Recording deleted files..");
	deleted = FS_PatchOpen ("deleted.bat", "w");
	if (!deleted)
	{
		Com_Printf ("Unable to open %s for reading\n", "deleted.bat");
		FS_FreePatchEntries (entries, num_entries);
		return;
	}
	for (i = 0; i < num_entries; i++)
	{
		if (!entries[i])
			continue;
		fprintf (deleted, "del %s\n", entries[i]->path);
		free (entries[i]);
		entries[i] = NULL;
	}
	Com_Printf (" done.\n");
	free (entries);
	fclose (deleted);
	Com_Printf ("\nPatch complete.\n\n");
}


void FS_InitPatchCommands (void)
{
	fs_patch_snapshotfile = Cvar_Get("patch_snapshotfile", "snapshot.txt", CVAR_ARCHIVE);
	Cmd_AddCommand("patch_snapshot", FS_PatchSnapshot_f);
	Cmd_AddCommand("patch_create", FS_PatchCreate_f);
}
#endif
