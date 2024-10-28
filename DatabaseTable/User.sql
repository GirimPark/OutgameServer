/*DROP TABLE IF EXISTS [dbo].[User];*/
IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[User]') AND type in (N'U'))
BEGIN
    CREATE TABLE [dbo].[User] (
        Id INT PRIMARY KEY IDENTITY(1,1),
        Nickname NVARCHAR(255) COLLATE SQL_Latin1_General_CP1_CS_AS NOT NULL UNIQUE,
        Password NVARCHAR(255) NOT NULL,
        State INT NOT NULL DEFAULT 0
    );
END