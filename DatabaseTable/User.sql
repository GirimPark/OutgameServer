/*IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[User]') AND type in (N'U'))*/
DROP TABLE IF EXISTS [dbo].[User];
BEGIN
    CREATE TABLE [dbo].[User] (
        id INT PRIMARY KEY IDENTITY(1,1),
        username NVARCHAR(255) NOT NULL UNIQUE,
        password NVARCHAR(255) NOT NULL,
        state INT NOT NULL DEFAULT 0
    );
END